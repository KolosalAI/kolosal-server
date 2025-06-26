#include "kolosal/document_service.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "inference_interface.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

namespace kolosal
{

class DocumentService::Impl
{
public:
    DatabaseConfig config_;
    std::unique_ptr<QdrantClient> qdrant_client_;
    bool initialized_ = false;
    std::mutex mutex_;
    
    Impl(const DatabaseConfig& config) : config_(config)
    {
        if (config_.qdrant.enabled)
        {
            QdrantClient::Config client_config;
            client_config.host = config_.qdrant.host;
            client_config.port = config_.qdrant.port;
            client_config.apiKey = config_.qdrant.apiKey;
            client_config.timeout = config_.qdrant.timeout;
            client_config.maxConnections = config_.qdrant.maxConnections;
            client_config.connectionTimeout = config_.qdrant.connectionTimeout;
            
            qdrant_client_ = std::make_unique<QdrantClient>(client_config);
            
            ServerLogger::logInfo("DocumentService initialized with Qdrant client");
        }
        else
        {
            ServerLogger::logWarning("DocumentService initialized but Qdrant is disabled in configuration");
        }
    }
    
    std::string generateDocumentId()
    {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_int_distribution<> dis(0, 15);
        
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        std::stringstream ss;
        ss << "doc_" << std::hex << timestamp << "_";
        
        for (int i = 0; i < 8; ++i)
        {
            ss << std::hex << dis(gen);
        }
        
        return ss.str();
    }
    
    std::future<std::vector<float>> generateEmbedding(const std::string& text, const std::string& model_id)
    {
        return std::async(std::launch::async, [this, text, model_id]() -> std::vector<float> {
            try
            {
                std::string effective_model_id = model_id.empty() ? config_.qdrant.defaultEmbeddingModel : model_id;
                
                ServerLogger::logDebug("Generating embedding for text (length: %zu) using model: %s", 
                                       text.length(), effective_model_id.c_str());
                
                // Get the NodeManager and inference engine
                auto& nodeManager = ServerAPI::instance().getNodeManager();
                auto engine = nodeManager.getEngine(effective_model_id);
                
                if (!engine)
                {
                    throw std::runtime_error("Embedding model '" + effective_model_id + "' not found or could not be loaded");
                }
                
                // Prepare embedding parameters
                EmbeddingParameters params;
                params.input = text;
                params.seqId = 0; // Use a default sequence ID for document embeddings
                
                if (!params.isValid())
                {
                    throw std::runtime_error("Invalid embedding parameters");
                }
                
                // Submit embedding job
                int jobId = engine->submitEmbeddingJob(params);
                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit embedding job to inference engine");
                }
                
                ServerLogger::logDebug("Submitted embedding job %d for model '%s'", jobId, effective_model_id.c_str());
                
                // Wait for job completion with timeout
                const int max_wait_seconds = 30;
                int wait_count = 0;
                
                while (!engine->isJobFinished(jobId) && wait_count < max_wait_seconds * 10)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    wait_count++;
                }
                
                if (!engine->isJobFinished(jobId))
                {
                    throw std::runtime_error("Embedding job timed out after " + std::to_string(max_wait_seconds) + " seconds");
                }
                
                // Check for errors
                if (engine->hasJobError(jobId))
                {
                    std::string error = engine->getJobError(jobId);
                    throw std::runtime_error("Inference error: " + error);
                }
                
                // Get the embedding result
                EmbeddingResult result = engine->getEmbeddingResult(jobId);
                
                if (result.embedding.empty())
                {
                    throw std::runtime_error("Empty embedding result from inference engine");
                }
                
                ServerLogger::logDebug("Generated embedding with %zu dimensions", result.embedding.size());
                
                return result.embedding;
            }
            catch (const std::exception& ex)
            {
                ServerLogger::logError("Error generating embedding: %s", ex.what());
                throw;
            }
        });
    }
    
    std::future<bool> ensureCollection(const std::string& collection_name, int vector_size)
    {
        return std::async(std::launch::async, [this, collection_name, vector_size]() -> bool {
            try
            {
                // Check if collection exists
                auto exists_result = qdrant_client_->collectionExists(collection_name).get();
                
                if (exists_result.success)
                {
                    ServerLogger::logDebug("Collection '%s' already exists", collection_name.c_str());
                    return true;
                }
                
                // Create collection
                ServerLogger::logInfo("Creating collection '%s' with vector size %d", collection_name.c_str(), vector_size);
                auto create_result = qdrant_client_->createCollection(collection_name, vector_size, "Cosine").get();
                
                if (!create_result.success)
                {
                    ServerLogger::logError("Failed to create collection '%s': %s", 
                                           collection_name.c_str(), create_result.error_message.c_str());
                    return false;
                }
                
                ServerLogger::logInfo("Successfully created collection '%s'", collection_name.c_str());
                return true;
            }
            catch (const std::exception& ex)
            {
                ServerLogger::logError("Error ensuring collection '%s': %s", collection_name.c_str(), ex.what());
                return false;
            }
        });
    }
};

// DocumentService implementations
DocumentService::DocumentService(const DatabaseConfig& database_config)
    : pImpl(std::make_unique<Impl>(database_config))
{
}

DocumentService::~DocumentService() = default;

DocumentService::DocumentService(DocumentService&&) noexcept = default;
DocumentService& DocumentService::operator=(DocumentService&&) noexcept = default;

std::future<bool> DocumentService::initialize()
{
    return std::async(std::launch::async, [this]() -> bool {
        std::lock_guard<std::mutex> lock(pImpl->mutex_);
        
        if (pImpl->initialized_)
        {
            return true;
        }
        
        if (!pImpl->config_.qdrant.enabled)
        {
            ServerLogger::logWarning("DocumentService: Qdrant is disabled, skipping initialization");
            pImpl->initialized_ = true;
            return true;
        }
        
        if (!pImpl->qdrant_client_)
        {
            ServerLogger::logError("DocumentService: Qdrant client not initialized");
            return false;
        }
        
        try
        {
            // Test connection
            ServerLogger::logInfo("DocumentService: Testing Qdrant connection...");
            auto connection_result = pImpl->qdrant_client_->testConnection().get();
            
            if (!connection_result.success)
            {
                ServerLogger::logError("DocumentService: Failed to connect to Qdrant: %s", 
                                       connection_result.error_message.c_str());
                return false;
            }
            
            ServerLogger::logInfo("DocumentService: Successfully connected to Qdrant at %s:%d",
                                  pImpl->config_.qdrant.host.c_str(), pImpl->config_.qdrant.port);
            
            pImpl->initialized_ = true;
            return true;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("DocumentService: Initialization failed: %s", ex.what());
            return false;
        }
    });
}

std::future<AddDocumentsResponse> DocumentService::addDocuments(const AddDocumentsRequest& request)
{
    return std::async(std::launch::async, [this, request]() -> AddDocumentsResponse {
        AddDocumentsResponse response;
        
        try
        {
            if (!pImpl->initialized_)
            {
                throw std::runtime_error("DocumentService not initialized");
            }
            
            if (!pImpl->config_.qdrant.enabled)
            {
                throw std::runtime_error("Qdrant is disabled in configuration");
            }
            
            std::string collection_name = request.collection_name.empty() 
                ? pImpl->config_.qdrant.collectionName 
                : request.collection_name;
            
            response.collection_name = collection_name;
            
            ServerLogger::logInfo("Processing %zu documents for collection '%s'", 
                                  request.documents.size(), collection_name.c_str());
            
            // Generate embeddings for all documents
            std::vector<std::future<std::vector<float>>> embedding_futures;
            std::vector<std::string> document_ids;
            
            for (size_t i = 0; i < request.documents.size(); ++i)
            {
                std::string doc_id = pImpl->generateDocumentId();
                document_ids.push_back(doc_id);
                
                embedding_futures.push_back(
                    pImpl->generateEmbedding(request.documents[i].text, "")
                );
            }
            
            // Wait for all embeddings and prepare points
            std::vector<QdrantPoint> points;
            int vector_size = 0;
            
            for (size_t i = 0; i < embedding_futures.size(); ++i)
            {
                try
                {
                    auto embedding = embedding_futures[i].get();
                    
                    if (vector_size == 0)
                    {
                        vector_size = static_cast<int>(embedding.size());
                    }
                    else if (static_cast<int>(embedding.size()) != vector_size)
                    {
                        throw std::runtime_error("Inconsistent embedding dimensions");
                    }
                    
                    QdrantPoint point;
                    point.id = document_ids[i];
                    point.vector = std::move(embedding);
                    
                    // Add document metadata
                    point.payload["text"] = request.documents[i].text;
                    for (const auto& [key, value] : request.documents[i].metadata)
                    {
                        point.payload[key] = value;
                    }
                    
                    // Add timestamp
                    auto now = std::chrono::system_clock::now();
                    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                    point.payload["indexed_at"] = timestamp;
                    
                    points.push_back(std::move(point));
                    response.addSuccess(document_ids[i]);
                }
                catch (const std::exception& ex)
                {
                    ServerLogger::logError("Failed to generate embedding for document %zu: %s", i, ex.what());
                    response.addFailure("Failed to generate embedding: " + std::string(ex.what()));
                }
            }
            
            if (points.empty())
            {
                throw std::runtime_error("No documents could be processed");
            }
            
            // Ensure collection exists
            bool collection_ready = pImpl->ensureCollection(collection_name, vector_size).get();
            if (!collection_ready)
            {
                throw std::runtime_error("Failed to create or access collection '" + collection_name + "'");
            }
            
            // Upsert points to Qdrant
            ServerLogger::logInfo("Upserting %zu points to collection '%s'", points.size(), collection_name.c_str());
            auto upsert_result = pImpl->qdrant_client_->upsertPoints(collection_name, points).get();
            
            if (!upsert_result.success)
            {
                throw std::runtime_error("Failed to upsert points to Qdrant: " + upsert_result.error_message);
            }
            
            ServerLogger::logInfo("Successfully indexed %d documents to collection '%s'", 
                                  response.successful_count, collection_name.c_str());
            
            return response;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error in addDocuments: %s", ex.what());
            
            // If we had some successes but failed at the end, still return partial results
            if (response.successful_count == 0)
            {
                response.addFailure("Service error: " + std::string(ex.what()));
            }
            
            return response;
        }
    });
}

std::future<bool> DocumentService::testConnection()
{
    return std::async(std::launch::async, [this]() -> bool {
        if (!pImpl->config_.qdrant.enabled || !pImpl->qdrant_client_)
        {
            return false;
        }
        
        try
        {
            auto result = pImpl->qdrant_client_->testConnection().get();
            return result.success;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("DocumentService connection test failed: %s", ex.what());
            return false;
        }
    });
}

std::future<std::vector<float>> DocumentService::getEmbedding(const std::string& text, const std::string& model_id)
{
    return pImpl->generateEmbedding(text, model_id);
}

} // namespace kolosal

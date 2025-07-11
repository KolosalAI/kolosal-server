#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/retrieval/remove_document_types.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "inference_interface.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <set>

namespace kolosal
{
namespace retrieval
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
        static thread_local std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
        static thread_local std::uniform_int_distribution<uint16_t> dis16(0, 0xFFFF);
        static thread_local std::uniform_int_distribution<unsigned int> dis8(0, 0xFF);
        
        // Generate UUID v4 (random) - format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        // where x is any hex digit and y is one of 8, 9, A, or B
        
        uint32_t time_low = dis(gen);
        uint16_t time_mid = dis16(gen);
        uint16_t time_hi_and_version = (dis16(gen) & 0x0FFF) | 0x4000; // Version 4
        uint8_t clock_seq_hi_and_reserved = static_cast<uint8_t>((dis8(gen) & 0x3F) | 0x80); // Variant bits
        uint8_t clock_seq_low = static_cast<uint8_t>(dis8(gen));
        uint64_t node = (static_cast<uint64_t>(dis(gen)) << 16) | dis16(gen);
        
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << time_low << "-";
        ss << std::setw(4) << time_mid << "-";
        ss << std::setw(4) << time_hi_and_version << "-";
        ss << std::setw(2) << static_cast<int>(clock_seq_hi_and_reserved);
        ss << std::setw(2) << static_cast<int>(clock_seq_low) << "-";
        ss << std::setw(12) << node;
        
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
                
                ServerLogger::logDebug("Waiting for embedding job %d to complete", jobId);
                
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
            
            std::string collection_name = "documents"; // Always use "documents" collection
            
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

std::future<RetrieveResponse> DocumentService::retrieveDocuments(const RetrieveRequest& request)
{
    return std::async(std::launch::async, [this, request]() -> RetrieveResponse {
        RetrieveResponse response;
        
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
            
            std::string collection_name = "documents"; // Always use "documents" collection
            
            response.query = request.query;
            response.k = request.k;
            response.collection_name = collection_name;
            response.score_threshold = request.score_threshold;
            
            ServerLogger::logInfo("Retrieving documents for query: '%s' (k=%d, collection='%s')", 
                                  request.query.c_str(), request.k, collection_name.c_str());
            
            // Generate embedding for the query
            auto query_embedding_future = pImpl->generateEmbedding(request.query, "");
            std::vector<float> query_embedding = query_embedding_future.get();
            
            if (query_embedding.empty())
            {
                throw std::runtime_error("Failed to generate embedding for query");
            }
            
            ServerLogger::logDebug("Generated query embedding with %zu dimensions", query_embedding.size());
            
            // Perform vector search
            auto search_result = pImpl->qdrant_client_->search(
                collection_name, 
                query_embedding, 
                request.k, 
                request.score_threshold
            ).get();
            
            if (!search_result.success)
            {
                throw std::runtime_error("Vector search failed: " + search_result.error_message);
            }
            
            // Parse search results
            if (search_result.response_data.contains("result") && search_result.response_data["result"].is_array())
            {
                auto results = search_result.response_data["result"];
                
                for (const auto& result_item : results)
                {
                    if (result_item.contains("id") && result_item.contains("score") && 
                        result_item.contains("payload"))
                    {
                        RetrievedDocument doc;
                        doc.id = result_item["id"].get<std::string>();
                        doc.score = result_item["score"].get<float>();
                        
                        // Extract text from payload
                        auto payload = result_item["payload"];
                        if (payload.contains("text"))
                        {
                            doc.text = payload["text"].get<std::string>();
                        }
                        
                        // Extract metadata (exclude text field)
                        for (auto& [key, value] : payload.items())
                        {
                            if (key != "text")
                            {
                                doc.metadata[key] = value;
                            }
                        }
                        
                        response.addDocument(doc);
                    }
                }
            }
            
            ServerLogger::logInfo("Successfully retrieved %d documents for query", response.total_found);
            
            return response;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error retrieving documents: %s", ex.what());
            throw;
        }
    });
}

std::future<RemoveDocumentsResponse> DocumentService::removeDocuments(const RemoveDocumentsRequest& request)
{
    return std::async(std::launch::async, [this, request]() -> RemoveDocumentsResponse {
        RemoveDocumentsResponse response;
        
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
            
            std::string collection_name = "documents"; // Always use "documents" collection
            
            response.collection_name = collection_name;
            
            ServerLogger::logInfo("Removing %zu documents from collection '%s'", 
                                  request.ids.size(), collection_name.c_str());
            
            // Check if collection exists
            auto exists_result = pImpl->qdrant_client_->collectionExists(collection_name).get();
            if (!exists_result.success)
            {
                // Collection doesn't exist, all IDs are "not found"
                ServerLogger::logWarning("Collection '%s' does not exist, marking all documents as not found", 
                                         collection_name.c_str());
                for (const auto& id : request.ids)
                {
                    response.addNotFound(id);
                }
                return response;
            }
            
            // First, check which documents exist by retrieving them
            ServerLogger::logDebug("Checking existence of %zu documents before deletion", request.ids.size());
            auto get_result = pImpl->qdrant_client_->getPoints(collection_name, request.ids).get();
            
            std::vector<std::string> existing_ids;
            std::vector<std::string> not_found_ids;
            
            if (get_result.success && get_result.response_data.contains("result"))
            {
                auto results = get_result.response_data["result"];
                if (results.is_array())
                {
                    // Track which IDs were found
                    std::set<std::string> found_ids;
                    for (const auto& result_item : results)
                    {
                        if (result_item.contains("id") && !result_item["id"].is_null())
                        {
                            std::string found_id = result_item["id"].get<std::string>();
                            found_ids.insert(found_id);
                            existing_ids.push_back(found_id);
                        }
                    }
                    
                    // Identify which IDs were not found
                    for (const auto& requested_id : request.ids)
                    {
                        if (found_ids.find(requested_id) == found_ids.end())
                        {
                            not_found_ids.push_back(requested_id);
                        }
                    }
                }
            }
            else
            {
                ServerLogger::logWarning("Failed to check document existence: %s", get_result.error_message.c_str());
                // If we can't check existence, treat all as not found
                not_found_ids = request.ids;
            }
            
            // Mark not found documents
            for (const auto& id : not_found_ids)
            {
                response.addNotFound(id);
            }
            
            ServerLogger::logInfo("Found %zu existing documents, %zu not found", 
                                  existing_ids.size(), not_found_ids.size());
            
            // Only attempt to delete existing documents
            if (!existing_ids.empty())
            {
                auto delete_result = pImpl->qdrant_client_->deletePoints(collection_name, existing_ids).get();
                
                if (delete_result.success)
                {
                    // Mark all existing documents as successfully removed
                    for (const auto& id : existing_ids)
                    {
                        response.addRemoved(id);
                    }
                    
                    ServerLogger::logInfo("Successfully deleted %zu document IDs from collection '%s'", 
                                          existing_ids.size(), collection_name.c_str());
                }
                else
                {
                    // Delete operation failed, mark all existing documents as failed
                    ServerLogger::logError("Failed to delete points from Qdrant: %s", 
                                           delete_result.error_message.c_str());
                    
                    for (const auto& id : existing_ids)
                    {
                        response.addFailed(id);
                    }
                }
            }
            
            return response;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error in removeDocuments: %s", ex.what());
            
            // If we had some successes but failed at the end, still return partial results
            if (response.removed_count == 0 && response.not_found_count == 0)
            {
                // Mark all as failed if no operation succeeded
                for (const auto& id : request.ids)
                {
                    response.addFailed(id);
                }
            }
            
            return response;
        }
    });
}

} // namespace retrieval
} // namespace kolosal

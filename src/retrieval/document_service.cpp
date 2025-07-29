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
#include <algorithm>
#include <algorithm>

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

    // New batch embedding generation method
    std::future<std::vector<std::pair<size_t, std::vector<float>>>> generateEmbeddingsBatch(
        const std::vector<std::pair<size_t, std::string>>& texts, const std::string& model_id)
    {
        return std::async(std::launch::async, [this, texts, model_id]() -> std::vector<std::pair<size_t, std::vector<float>>> {
            std::vector<std::pair<size_t, std::vector<float>>> results;
            
            if (texts.empty()) {
                return results;
            }
            
            try {
                std::string effective_model_id = model_id.empty() ? config_.qdrant.defaultEmbeddingModel : model_id;
                
                ServerLogger::logInfo("Generating embeddings for batch of %zu texts using model: %s", 
                                     texts.size(), effective_model_id.c_str());
                
                // Get the NodeManager and inference engine
                auto& nodeManager = ServerAPI::instance().getNodeManager();
                auto engine = nodeManager.getEngine(effective_model_id);
                
                if (!engine) {
                    throw std::runtime_error("Embedding model '" + effective_model_id + "' not found or could not be loaded");
                }
                
                // Process texts in this batch concurrently, but limit concurrency to avoid resource exhaustion
                std::vector<std::future<std::vector<float>>> embedding_futures;
                
                for (const auto& [index, text] : texts) {
                    embedding_futures.push_back(std::async(std::launch::async, [this, text, effective_model_id]() -> std::vector<float> {
                        try {
                            // Get engine instance for this thread
                            auto& nodeManager = ServerAPI::instance().getNodeManager();
                            auto engine = nodeManager.getEngine(effective_model_id);
                            
                            if (!engine) {
                                throw std::runtime_error("Embedding model not available");
                            }
                            
                            // Prepare embedding parameters
                            EmbeddingParameters params;
                            params.input = text;
                            params.seqId = 0;
                            
                            if (!params.isValid()) {
                                throw std::runtime_error("Invalid embedding parameters");
                            }
                            
                            // Submit embedding job
                            int jobId = engine->submitEmbeddingJob(params);
                            if (jobId < 0) {
                                throw std::runtime_error("Failed to submit embedding job");
                            }
                            
                            // Wait for completion with timeout
                            const int max_wait_seconds = 30;
                            int wait_count = 0;
                            
                            while (!engine->isJobFinished(jobId) && wait_count < max_wait_seconds * 10) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                wait_count++;
                            }
                            
                            if (!engine->isJobFinished(jobId)) {
                                throw std::runtime_error("Embedding job timed out after " + std::to_string(max_wait_seconds) + " seconds");
                            }
                            
                            // Check for errors
                            if (engine->hasJobError(jobId)) {
                                std::string error = engine->getJobError(jobId);
                                throw std::runtime_error("Inference error: " + error);
                            }
                            
                            // Get result
                            EmbeddingResult result = engine->getEmbeddingResult(jobId);
                            
                            if (result.embedding.empty()) {
                                throw std::runtime_error("Empty embedding result");
                            }
                            
                            return result.embedding;
                        } catch (const std::exception& ex) {
                            ServerLogger::logError("Error generating embedding in batch: %s", ex.what());
                            throw;
                        }
                    }));
                }
                
                // Collect results
                for (size_t i = 0; i < embedding_futures.size(); ++i) {
                    try {
                        auto embedding = embedding_futures[i].get();
                        results.emplace_back(texts[i].first, std::move(embedding));
                    } catch (const std::exception& ex) {
                        ServerLogger::logError("Failed to get embedding result for text %zu in batch: %s", 
                                             texts[i].first, ex.what());
                        // Continue processing other embeddings
                    }
                }
                
                ServerLogger::logInfo("Completed batch embedding generation: %zu/%zu successful", 
                                     results.size(), texts.size());
                
                return results;
            } catch (const std::exception& ex) {
                ServerLogger::logError("Error in batch embedding generation: %s", ex.what());
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
                // Try to initialize automatically on first use
                ServerLogger::logInfo("DocumentService not initialized, attempting auto-initialization...");
                auto init_future = initialize();
                bool initialized = init_future.get();
                if (!initialized) {
                    throw std::runtime_error("DocumentService not initialized");
                }
            }
            
            if (!pImpl->config_.qdrant.enabled)
            {
                throw std::runtime_error("Qdrant is disabled in configuration");
            }
            
            std::string collection_name = "documents"; // Always use "documents" collection
            
            response.collection_name = collection_name;
            
            ServerLogger::logInfo("Processing %zu documents for collection '%s'", 
                                  request.documents.size(), collection_name.c_str());
            
            // Use batching to generate embeddings
            const int batch_size = pImpl->config_.qdrant.embeddingBatchSize;
            ServerLogger::logInfo("Using embedding batch size: %d", batch_size);
            
            std::vector<std::string> document_ids;
            std::vector<QdrantPoint> points;
            int vector_size = 0;
            
            // Generate document IDs first
            for (size_t i = 0; i < request.documents.size(); ++i) {
                document_ids.push_back(pImpl->generateDocumentId());
            }
            
            // Process documents in batches
            for (size_t batch_start = 0; batch_start < request.documents.size(); batch_start += batch_size)
            {
                size_t batch_end = std::min(batch_start + batch_size, request.documents.size());
                size_t current_batch_size = batch_end - batch_start;
                
                ServerLogger::logInfo("Processing batch %zu-%zu (%zu documents)", 
                                     batch_start, batch_end - 1, current_batch_size);
                
                // Prepare texts for this batch with their original indices
                std::vector<std::pair<size_t, std::string>> batch_texts;
                for (size_t i = batch_start; i < batch_end; ++i) {
                    batch_texts.emplace_back(i, request.documents[i].text);
                }
                
                // Generate embeddings for this batch
                try {
                    auto batch_future = pImpl->generateEmbeddingsBatch(batch_texts, "");
                    auto batch_results = batch_future.get();
                    
                    // Process batch results
                    for (const auto& [original_index, embedding] : batch_results) {
                        try {
                            if (vector_size == 0) {
                                vector_size = static_cast<int>(embedding.size());
                            } else if (static_cast<int>(embedding.size()) != vector_size) {
                                throw std::runtime_error("Inconsistent embedding dimensions");
                            }
                            
                            QdrantPoint point;
                            point.id = document_ids[original_index];
                            point.vector = embedding;
                            
                            // Add document metadata
                            point.payload["text"] = request.documents[original_index].text;
                            for (const auto& [key, value] : request.documents[original_index].metadata) {
                                point.payload[key] = value;
                            }
                            
                            // Add timestamp
                            auto now = std::chrono::system_clock::now();
                            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                            point.payload["indexed_at"] = timestamp;
                            
                            points.push_back(std::move(point));
                            response.addSuccess(document_ids[original_index]);
                            
                        } catch (const std::exception& ex) {
                            ServerLogger::logError("Failed to process embedding result for document %zu: %s", 
                                                 original_index, ex.what());
                            response.addFailure("Failed to process embedding: " + std::string(ex.what()));
                        }
                    }
                    
                    // Handle any documents in this batch that failed to get embeddings
                    std::set<size_t> successful_indices;
                    for (const auto& [index, _] : batch_results) {
                        successful_indices.insert(index);
                    }
                    
                    for (size_t i = batch_start; i < batch_end; ++i) {
                        if (successful_indices.find(i) == successful_indices.end()) {
                            ServerLogger::logError("Failed to generate embedding for document %zu", i);
                            response.addFailure("Failed to generate embedding");
                        }
                    }
                    
                } catch (const std::exception& ex) {
                    ServerLogger::logError("Failed to process embedding batch %zu-%zu: %s", 
                                         batch_start, batch_end - 1, ex.what());
                    
                    // Mark all documents in this batch as failed
                    for (size_t i = batch_start; i < batch_end; ++i) {
                        response.addFailure("Batch processing failed: " + std::string(ex.what()));
                    }
                }
                
                // Add a small delay between batches to prevent overwhelming the system
                if (batch_end < request.documents.size()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

std::future<nlohmann::json> DocumentService::getHealthStatus()
{
    return std::async(std::launch::async, [this]() -> nlohmann::json {
        nlohmann::json health;
        health["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Basic service status
        health["service_initialized"] = pImpl->initialized_;
        health["qdrant_enabled"] = pImpl->config_.qdrant.enabled;
        
        // Connection status
        bool connection_ok = false;
        std::string connection_error = "";
        try 
        {
            if (pImpl->config_.qdrant.enabled && pImpl->qdrant_client_)
            {
                auto result = pImpl->qdrant_client_->testConnection().get();
                connection_ok = result.success;
                if (!result.success)
                {
                    connection_error = result.error_message;
                }
            }
            else
            {
                connection_error = "Qdrant disabled or client not initialized";
            }
        }
        catch (const std::exception& ex)
        {
            connection_error = ex.what();
        }
        
        health["qdrant_connection"] = connection_ok;
        if (!connection_error.empty())
        {
            health["connection_error"] = connection_error;
        }
        
        // Collection status
        std::string collection_name = "documents";
        bool collection_exists = false;
        std::string collection_error = "";
        
        try
        {
            if (connection_ok)
            {
                auto exists_result = pImpl->qdrant_client_->collectionExists(collection_name).get();
                collection_exists = exists_result.success;
                if (!exists_result.success)
                {
                    collection_error = exists_result.error_message;
                }
            }
            else
            {
                collection_error = "Cannot check collection - no database connection";
            }
        }
        catch (const std::exception& ex)
        {
            collection_error = ex.what();
        }
        
        health["collection_exists"] = collection_exists;
        health["collection_name"] = collection_name;
        if (!collection_error.empty())
        {
            health["collection_error"] = collection_error;
        }
        
        // Configuration info
        nlohmann::json config_info;
        config_info["host"] = pImpl->config_.qdrant.host;
        config_info["port"] = pImpl->config_.qdrant.port;
        config_info["timeout"] = pImpl->config_.qdrant.timeout;
        config_info["default_embedding_model"] = pImpl->config_.qdrant.defaultEmbeddingModel;
        health["configuration"] = config_info;
        
        // Overall status
        health["status"] = (pImpl->initialized_ && connection_ok && collection_exists) ? "healthy" : "unhealthy";
        
        // Recommendations
        nlohmann::json recommendations = nlohmann::json::array();
        if (!pImpl->initialized_)
        {
            recommendations.push_back("Initialize the DocumentService");
        }
        if (!pImpl->config_.qdrant.enabled)
        {
            recommendations.push_back("Enable Qdrant in configuration");
        }
        if (!connection_ok)
        {
            recommendations.push_back("Check Qdrant server is running and accessible at " + 
                                    pImpl->config_.qdrant.host + ":" + std::to_string(pImpl->config_.qdrant.port));
        }
        if (!collection_exists && connection_ok)
        {
            recommendations.push_back("Index some documents to create the '" + collection_name + "' collection");
        }
        
        if (!recommendations.empty())
        {
            health["recommendations"] = recommendations;
        }
        
        return health;
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
                // Try to initialize automatically on first use
                ServerLogger::logInfo("DocumentService not initialized, attempting auto-initialization...");
                auto init_future = initialize();
                bool initialized = init_future.get();
                if (!initialized) {
                    throw std::runtime_error("DocumentService not initialized - please ensure the service is properly started");
                }
            }
            
            if (!pImpl->config_.qdrant.enabled)
            {
                throw std::runtime_error("Qdrant is disabled in configuration - enable Qdrant to use document retrieval");
            }
            
            std::string collection_name = "documents"; // Always use "documents" collection
            
            response.query = request.query;
            response.k = request.k;
            response.collection_name = collection_name;
            response.score_threshold = request.score_threshold;
            
            ServerLogger::logInfo("Retrieving documents for query: '%s' (k=%d, collection='%s', threshold=%.3f)", 
                                  request.query.c_str(), request.k, collection_name.c_str(), request.score_threshold);
            
            // Check if collection exists before proceeding
            auto collection_exists_result = pImpl->qdrant_client_->collectionExists(collection_name).get();
            if (!collection_exists_result.success)
            {
                ServerLogger::logWarning("Collection '%s' does not exist - no documents have been indexed yet", 
                                       collection_name.c_str());
                // Return empty response with helpful message
                ServerLogger::logInfo("No documents found - collection '%s' is empty or doesn't exist", collection_name.c_str());
                return response;
            }
            
            // Generate embedding for the query
            ServerLogger::logDebug("Generating embedding for query: '%s'", request.query.c_str());
            auto query_embedding_future = pImpl->generateEmbedding(request.query, "");
            std::vector<float> query_embedding;
            
            try 
            {
                query_embedding = query_embedding_future.get();
            }
            catch (const std::exception& ex)
            {
                throw std::runtime_error("Failed to generate embedding for query: " + std::string(ex.what()) + 
                                       " - check if the embedding model is available and properly configured");
            }
            
            if (query_embedding.empty())
            {
                throw std::runtime_error("Generated embedding is empty - this may indicate an issue with the embedding model");
            }
            
            ServerLogger::logDebug("Generated query embedding with %zu dimensions", query_embedding.size());
            
            // Perform vector search
            ServerLogger::logDebug("Performing vector search in collection '%s'", collection_name.c_str());
            auto search_result = pImpl->qdrant_client_->search(
                collection_name, 
                query_embedding, 
                request.k, 
                request.score_threshold
            ).get();
            
            if (!search_result.success)
            {
                // Provide more specific error information
                std::string detailed_error = "Vector search failed: " + search_result.error_message;
                if (search_result.status_code == 404)
                {
                    detailed_error += " - Collection may not exist or may be empty";
                }
                else if (search_result.status_code >= 500)
                {
                    detailed_error += " - Qdrant server error, check if Qdrant is running and accessible";
                }
                throw std::runtime_error(detailed_error);
            }
            
            // Parse search results
            int documents_parsed = 0;
            if (search_result.response_data.contains("result") && search_result.response_data["result"].is_array())
            {
                auto results = search_result.response_data["result"];
                ServerLogger::logDebug("Search returned %zu potential results", results.size());
                
                for (const auto& result_item : results)
                {
                    try
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
                            else
                            {
                                ServerLogger::logWarning("Document %s missing 'text' field in payload", doc.id.c_str());
                                doc.text = ""; // Set empty text rather than skip the document
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
                            documents_parsed++;
                            
                            ServerLogger::logDebug("Added document %s with score %.3f", 
                                                 doc.id.c_str(), doc.score);
                        }
                        else
                        {
                            ServerLogger::logWarning("Skipping malformed search result item (missing id, score, or payload)");
                        }
                    }
                    catch (const std::exception& ex)
                    {
                        ServerLogger::logError("Error parsing search result item: %s", ex.what());
                        // Continue processing other results
                    }
                }
            }
            else
            {
                ServerLogger::logWarning("Search response does not contain valid 'result' array");
            }
            
            // Log final results
            if (response.total_found == 0)
            {
                if (documents_parsed == 0 && search_result.response_data.contains("result"))
                {
                    auto results = search_result.response_data["result"];
                    if (results.is_array() && results.size() == 0)
                    {
                        ServerLogger::logInfo("No documents found matching query '%s' with score threshold %.3f", 
                                            request.query.c_str(), request.score_threshold);
                    }
                    else
                    {
                        ServerLogger::logWarning("Search returned results but none could be parsed successfully");
                    }
                }
                else
                {
                    ServerLogger::logInfo("No documents found - this could be due to: empty collection, high score threshold (%.3f), or query not matching indexed content", 
                                        request.score_threshold);
                }
            }
            else
            {
                ServerLogger::logInfo("Successfully retrieved %d documents for query (score threshold: %.3f)", 
                                    response.total_found, request.score_threshold);
            }
            
            return response;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error retrieving documents for query '%s': %s", 
                                 request.query.c_str(), ex.what());
            
            // Provide helpful debugging information
            ServerLogger::logDebug("Retrieval error details - Collection: %s, K: %d, Threshold: %.3f", 
                                 response.collection_name.c_str(), request.k, request.score_threshold);
            
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
                // Try to initialize automatically on first use
                ServerLogger::logInfo("DocumentService not initialized, attempting auto-initialization...");
                auto init_future = initialize();
                bool initialized = init_future.get();
                if (!initialized) {
                    throw std::runtime_error("DocumentService not initialized");
                }
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

std::future<std::vector<std::string>> DocumentService::listDocuments(const std::string& collection_name)
{
    return std::async(std::launch::async, [this, collection_name]() -> std::vector<std::string> {
        try
        {
            std::string effective_collection_name = collection_name.empty() ? 
                pImpl->config_.qdrant.collectionName : collection_name;
            
            std::vector<std::string> all_ids;
            std::string offset = "";
            const int batch_size = 1000;
            
            ServerLogger::logDebug("Starting to list documents from collection '%s'", 
                                   effective_collection_name.c_str());
            
            // Use scroll API to get all points
            do {
                auto result_future = pImpl->qdrant_client_->scrollPoints(effective_collection_name, batch_size, offset);
                QdrantResult result = result_future.get();
                
                if (!result.success)
                {
                    throw std::runtime_error("Failed to scroll points: " + result.error_message);
                }
                
                // Parse response to extract point IDs
                if (!result.response_data.contains("result") || 
                    !result.response_data["result"].contains("points") ||
                    !result.response_data["result"]["points"].is_array())
                {
                    break; // No more points
                }
                
                auto points = result.response_data["result"]["points"];
                if (points.empty())
                {
                    break;
                }
                
                // Extract IDs from points
                for (const auto& point : points)
                {
                    if (point.contains("id"))
                    {
                        std::string id = point["id"].is_string() ? 
                            point["id"].get<std::string>() : 
                            std::to_string(point["id"].get<int64_t>());
                        all_ids.push_back(id);
                    }
                }
                
                // Update offset for next batch
                if (result.response_data["result"].contains("next_page_offset") &&
                    !result.response_data["result"]["next_page_offset"].is_null())
                {
                    if (result.response_data["result"]["next_page_offset"].is_string())
                    {
                        offset = result.response_data["result"]["next_page_offset"].get<std::string>();
                    }
                    else if (result.response_data["result"]["next_page_offset"].is_number())
                    {
                        offset = std::to_string(result.response_data["result"]["next_page_offset"].get<int64_t>());
                    }
                    else
                    {
                        break; // No more pages
                    }
                }
                else
                {
                    break; // No next page
                }
                
            } while (!offset.empty() && offset != "null");
            
            ServerLogger::logInfo("Listed %zu documents from collection '%s'", 
                                  all_ids.size(), effective_collection_name.c_str());
            
            return all_ids;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error listing documents: %s", ex.what());
            throw;
        }
    });
}

std::future<std::vector<std::pair<std::string, std::optional<std::pair<std::string, std::unordered_map<std::string, nlohmann::json>>>>>> 
DocumentService::getDocumentsInfo(const std::vector<std::string>& ids, const std::string& collection_name)
{
    return std::async(std::launch::async, [this, ids, collection_name]() {
        try
        {
            std::string effective_collection_name = collection_name.empty() ? 
                pImpl->config_.qdrant.collectionName : collection_name;
            
            ServerLogger::logDebug("Getting info for %zu documents from collection '%s'", 
                                   ids.size(), effective_collection_name.c_str());
            
            std::vector<std::pair<std::string, std::optional<std::pair<std::string, std::unordered_map<std::string, nlohmann::json>>>>> results;
            
            // Process in batches to avoid overwhelming the server
            const size_t batch_size = 100;
            for (size_t i = 0; i < ids.size(); i += batch_size)
            {
                std::vector<std::string> batch_ids;
                for (size_t j = i; j < std::min(i + batch_size, ids.size()); ++j)
                {
                    batch_ids.push_back(ids[j]);
                }
                
                auto result_future = pImpl->qdrant_client_->getPoints(effective_collection_name, batch_ids);
                QdrantResult result = result_future.get();
                
                if (!result.success)
                {
                    ServerLogger::logWarning("Failed to get batch of points: %s", result.error_message.c_str());
                    
                    // Mark all points in this batch as not found
                    for (const auto& id : batch_ids)
                    {
                        results.emplace_back(id, std::nullopt);
                    }
                    continue;
                }
                
                // Parse response to extract point data
                std::set<std::string> found_ids;
                
                if (result.response_data.contains("result") &&
                    result.response_data["result"].is_array())
                {
                    for (const auto& point : result.response_data["result"])
                    {
                        if (point.contains("id") && point.contains("payload"))
                        {
                            std::string id = point["id"].is_string() ? 
                                point["id"].get<std::string>() : 
                                std::to_string(point["id"].get<int64_t>());
                            
                            found_ids.insert(id);
                            
                            // Extract text and metadata from payload
                            std::string text = "";
                            std::unordered_map<std::string, nlohmann::json> metadata;
                            
                            if (point["payload"].contains("text") && point["payload"]["text"].is_string())
                            {
                                text = point["payload"]["text"].get<std::string>();
                            }
                            
                            // Extract metadata (everything except text)
                            for (auto it = point["payload"].begin(); it != point["payload"].end(); ++it)
                            {
                                if (it.key() != "text")
                                {
                                    metadata[it.key()] = it.value();
                                }
                            }
                            
                            results.emplace_back(id, std::make_pair(text, metadata));
                        }
                    }
                }
                
                // Mark any IDs that weren't found
                for (const auto& id : batch_ids)
                {
                    if (found_ids.find(id) == found_ids.end())
                    {
                        results.emplace_back(id, std::nullopt);
                    }
                }
            }
            
            ServerLogger::logInfo("Retrieved info for %zu/%zu documents from collection '%s'", 
                                  std::count_if(results.begin(), results.end(), 
                                               [](const auto& pair) { return pair.second.has_value(); }),
                                  ids.size(), effective_collection_name.c_str());
            
            return results;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Error getting documents info: %s", ex.what());
            throw;
        }
    });
}

} // namespace retrieval
} // namespace kolosal

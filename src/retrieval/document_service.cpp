#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/retrieval/remove_document_types.hpp"
#include "kolosal/vector_database.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/server_config.hpp"
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
#include <numeric>
#include <cstdint>

namespace kolosal
{
namespace retrieval
{

class DocumentService::Impl
{
public:
    DatabaseConfig config_;
    std::unique_ptr<IVectorDatabase> vector_db_;
    bool initialized_ = false;
    std::mutex mutex_;
    
    Impl(const DatabaseConfig& config) : config_(config)
    {
        // Create vector database based on configuration
        try
        {
            nlohmann::json db_config;
            
            if (config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS)
            {
#ifdef USE_FAISS
                db_config["indexType"] = config_.faiss.indexType;
                db_config["indexPath"] = config_.faiss.indexPath;
                db_config["dimensions"] = config_.faiss.dimensions;
                db_config["normalizeVectors"] = config_.faiss.normalizeVectors;
                db_config["nlist"] = config_.faiss.nlist;
                db_config["nprobe"] = config_.faiss.nprobe;
                db_config["useGPU"] = config_.faiss.useGPU;
                db_config["gpuDevice"] = config_.faiss.gpuDevice;
                db_config["metricType"] = config_.faiss.metricType;
                
                vector_db_ = VectorDatabaseFactory::create(VectorDatabaseFactory::DatabaseType::FAISS, db_config);
                ServerLogger::logInfo("DocumentService initialized with FAISS vector database");
#else
                ServerLogger::logError("FAISS selected but not compiled in, attempting fallback to Qdrant");
                // Auto-enable Qdrant fallback if disabled
                if (!config_.qdrant.enabled)
                {
                    ServerLogger::logWarning("Qdrant disabled in configuration; enabling fallback with default parameters");
                    config_.qdrant.enabled = true;
                    if (config_.qdrant.host.empty()) config_.qdrant.host = "localhost";
                    if (config_.qdrant.port == 0) config_.qdrant.port = 6333;
                    if (config_.qdrant.collectionName.empty()) config_.qdrant.collectionName = "documents";
                    if (config_.qdrant.defaultEmbeddingModel.empty()) config_.qdrant.defaultEmbeddingModel = "text-embedding-3-small";
                    if (config_.qdrant.timeout == 0) config_.qdrant.timeout = 30;
                    if (config_.qdrant.maxConnections == 0) config_.qdrant.maxConnections = 10;
                    if (config_.qdrant.connectionTimeout == 0) config_.qdrant.connectionTimeout = 5;
                    if (config_.qdrant.embeddingBatchSize == 0) config_.qdrant.embeddingBatchSize = 5;
                    // Adjust primary vectorDatabase to reflect active backend
                    config_.vectorDatabase = DatabaseConfig::VectorDatabase::QDRANT;
                }
                // Proceed if enabled (either originally or just enabled)
                if (config_.qdrant.enabled)
                {
                    db_config["host"] = config_.qdrant.host;
                    db_config["port"] = config_.qdrant.port;
                    db_config["apiKey"] = config_.qdrant.apiKey;
                    db_config["timeout"] = config_.qdrant.timeout;
                    db_config["maxConnections"] = config_.qdrant.maxConnections;
                    db_config["connectionTimeout"] = config_.qdrant.connectionTimeout;
                    
                    vector_db_ = VectorDatabaseFactory::create(VectorDatabaseFactory::DatabaseType::QDRANT, db_config);
                    ServerLogger::logInfo("DocumentService initialized with Qdrant client (automatic fallback)");
                }
                else
                {
                    ServerLogger::logError("No vector database available after failed FAISS fallback");
                }
#endif
            }
            else // QDRANT
            {
                if (config_.qdrant.enabled)
                {
                    db_config["host"] = config_.qdrant.host;
                    db_config["port"] = config_.qdrant.port;
                    db_config["apiKey"] = config_.qdrant.apiKey;
                    db_config["timeout"] = config_.qdrant.timeout;
                    db_config["maxConnections"] = config_.qdrant.maxConnections;
                    db_config["connectionTimeout"] = config_.qdrant.connectionTimeout;
                    
                    vector_db_ = VectorDatabaseFactory::create(VectorDatabaseFactory::DatabaseType::QDRANT, db_config);
                    ServerLogger::logInfo("DocumentService initialized with Qdrant client");
                }
                else
                {
                    ServerLogger::logWarning("DocumentService initialized but Qdrant is disabled in configuration");
                }
            }
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("Failed to initialize vector database: %s", ex.what());
        }
    }
    
    // Decide which embedding model to use when none is explicitly provided
    std::string chooseEmbeddingModelId(const std::string& requested) const {
        if (!requested.empty()) {
            return requested;
        }
        try {
            // If user set a cross-backend retrieval model in config, prefer it
            if (!config_.retrievalEmbeddingModelId.empty()) {
                return config_.retrievalEmbeddingModelId;
            }
            if (config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS) {
                // Prefer an embedding model configured in ServerConfig models list
                const auto& cfg = ServerConfig::getInstance();
                for (const auto& model : cfg.models) {
                    if (!model.type.empty() && model.type == "embedding" && !model.id.empty()) {
                        return model.id;
                    }
                }
                // Fallback if none configured
                return std::string("text-embedding-3-small");
            } else {
                // Qdrant specific default if set
                if (!config_.qdrant.defaultEmbeddingModel.empty()) {
                    return config_.qdrant.defaultEmbeddingModel;
                }
                return std::string("text-embedding-3-small");
            }
        } catch (...) {
            return std::string("text-embedding-3-small");
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
        std::string effective_model_id = chooseEmbeddingModelId(model_id);
                
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
                std::string effective_model_id = chooseEmbeddingModelId(model_id);
                
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
                
                for (const auto& index_text : texts) {
                    const size_t index = index_text.first;
                    const std::string& text = index_text.second;
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
                if (!vector_db_)
                {
                    ServerLogger::logError("Vector database not initialized");
                    return false;
                }
                
                // Check if collection exists
                auto exists_result = vector_db_->collectionExists(collection_name).get();
                
                if (exists_result.success)
                {
                    ServerLogger::logDebug("Collection '%s' already exists", collection_name.c_str());
                    return true;
                }
                
                // Create collection
                ServerLogger::logInfo("Creating collection '%s' with vector size %d", collection_name.c_str(), vector_size);
                std::string distance_metric = (config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS && 
                                                config_.faiss.metricType == "IP") ? "IP" : "Cosine";
                auto create_result = vector_db_->createCollection(collection_name, vector_size, distance_metric).get();
                
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
        
        if (!pImpl->vector_db_)
        {
            ServerLogger::logError("DocumentService: Vector database not initialized");
            return false;
        }
        
        try
        {
            // Test connection
            std::string db_type = (pImpl->config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS) ? "FAISS" : "Qdrant";
            ServerLogger::logInfo("DocumentService: Testing %s connection...", db_type.c_str());
            auto connection_result = pImpl->vector_db_->testConnection().get();
            
            if (!connection_result.success)
            {
                ServerLogger::logError("DocumentService: Failed to connect to %s: %s", 
                                       db_type.c_str(), connection_result.error_message.c_str());
                return false;
            }
            
            if (pImpl->config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS)
            {
                ServerLogger::logInfo("DocumentService: Successfully initialized FAISS at %s", 
                                      pImpl->config_.faiss.indexPath.c_str());
                // Auto-create default collection if it does not exist yet
                const std::string default_collection = pImpl->config_.qdrant.collectionName.empty() ? "documents" : pImpl->config_.qdrant.collectionName;
                try {
                    auto exists = pImpl->vector_db_->collectionExists(default_collection).get();
                    if (!exists.success) {
                        ServerLogger::logInfo("FAISS default collection '%s' not found. Creating with dimension %d", 
                                              default_collection.c_str(), pImpl->config_.faiss.dimensions);
                        auto created = pImpl->vector_db_->createCollection(default_collection, pImpl->config_.faiss.dimensions, 
                            pImpl->config_.faiss.metricType == "IP" ? "IP" : "L2").get();
                        if (!created.success) {
                            ServerLogger::logError("Failed to create FAISS collection '%s': %s", default_collection.c_str(), created.error_message.c_str());
                            return false;
                        }
                    }
                } catch (const std::exception& ex) {
                    ServerLogger::logError("Error ensuring FAISS default collection: %s", ex.what());
                    return false;
                }
            }
            else
            {
                ServerLogger::logInfo("DocumentService: Successfully connected to Qdrant at %s:%d",
                                      pImpl->config_.qdrant.host.c_str(), pImpl->config_.qdrant.port);
            }
            
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
            
            if (!pImpl->vector_db_)
            {
                throw std::runtime_error("Vector database not initialized");
            }
            
            std::string collection_name = "documents"; // Always use "documents" collection
            
            response.collection_name = collection_name;
            
            ServerLogger::logInfo("Processing %zu documents for collection '%s'", 
                                  request.documents.size(), collection_name.c_str());
            
            // Use batching to generate embeddings
            const int batch_size = (pImpl->config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS ? 
                                     5 : pImpl->config_.qdrant.embeddingBatchSize);
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
                    for (const auto& pair_index_embedding : batch_results) {
                        const size_t original_index = pair_index_embedding.first;
                        const std::vector<float>& embedding = pair_index_embedding.second;
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
                            for (auto md_it = request.documents[original_index].metadata.begin();
                                 md_it != request.documents[original_index].metadata.end(); ++md_it) {
                                point.payload[md_it->first] = md_it->second;
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
                    for (const auto& pair_index_embedding : batch_results) {
                        successful_indices.insert(pair_index_embedding.first);
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
            
            // Upsert points to vector database
            ServerLogger::logInfo("Upserting %zu points to collection '%s'", points.size(), collection_name.c_str());
            
            // Convert QdrantPoint to VectorPoint
            std::vector<VectorPoint> vector_points;
            for (const auto& qpoint : points)
            {
                VectorPoint vpoint;
                vpoint.id = qpoint.id;
                vpoint.vector = qpoint.vector;
                vpoint.payload = qpoint.payload;
                vector_points.push_back(vpoint);
            }
            
            auto upsert_result = pImpl->vector_db_->upsertPoints(collection_name, vector_points).get();
            
            if (!upsert_result.success)
            {
                std::string db_type = (pImpl->config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS) ? "FAISS" : "Qdrant";
                throw std::runtime_error("Failed to upsert points to " + db_type + ": " + upsert_result.error_message);
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
        if (!pImpl->vector_db_)
        {
            return false;
        }
        
        try
        {
            auto result = pImpl->vector_db_->testConnection().get();
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
            auto search_result = pImpl->vector_db_->search(
                collection_name, 
                query_embedding, 
                request.k, 
                request.score_threshold
            ).get();
            
            if (!search_result.success)
            {
                std::string db_type = (pImpl->config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS) ? "FAISS" : "Qdrant";
                throw std::runtime_error("Vector search failed in " + db_type + ": " + search_result.error_message);
            }
            
            ServerLogger::logInfo("Found search results in vector database");
            
            // Process search results from response_data (works for both FAISS and Qdrant)
            if (search_result.response_data.contains("result") && search_result.response_data["result"].is_array())
            {
                auto results = search_result.response_data["result"];
                
                for (const auto& result_item : results)
                {
                    if (result_item.contains("id") && result_item.contains("score") && 
                        result_item.contains("payload"))
                    {
                        float score = result_item["score"].get<float>();
                        if (score < request.score_threshold)
                        {
                            continue; // Skip results below threshold
                        }
                        
                        RetrievedDocument doc;
                        doc.id = result_item["id"].get<std::string>();
                        doc.score = score;
                        
                        // Extract text from payload
                        auto payload = result_item["payload"];
                        if (payload.contains("text"))
                        {
                            doc.text = payload["text"].get<std::string>();
                        }
                        
                        // Extract metadata (exclude text field)
                        for (auto it = payload.begin(); it != payload.end(); ++it)
                        {
                            const std::string k = it.key();
                            if (k != "text")
                            {
                                doc.metadata[k] = it.value();
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
            
            if (!pImpl->vector_db_)
            {
                throw std::runtime_error("Vector database not initialized");
            }
            
            std::string collection_name = "documents"; // Always use "documents" collection
            
            response.collection_name = collection_name;
            
            ServerLogger::logInfo("Removing %zu documents from collection '%s'", 
                                  request.ids.size(), collection_name.c_str());
            
            // Check if collection exists
            auto exists_result = pImpl->vector_db_->collectionExists(collection_name).get();
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
            auto get_result = pImpl->vector_db_->getPoints(collection_name, request.ids).get();
            
            std::vector<std::string> existing_ids;
            std::vector<std::string> not_found_ids;
            
            if (get_result.success)
            {
                // Track which IDs were found using response_data (works for both FAISS and Qdrant)
                std::set<std::string> found_ids;
                if (get_result.response_data.contains("result") && get_result.response_data["result"].is_array())
                {
                    auto points = get_result.response_data["result"];
                    for (const auto& result_point : points)
                    {
                        if (result_point.contains("id"))
                        {
                            std::string id = result_point["id"].is_string() ? 
                                result_point["id"].get<std::string>() : 
                                std::to_string(result_point["id"].get<int64_t>());
                            found_ids.insert(id);
                            existing_ids.push_back(id);
                        }
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
                auto delete_result = pImpl->vector_db_->deletePoints(collection_name, existing_ids).get();
                
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
                    std::string db_type = (pImpl->config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS) ? "FAISS" : "Qdrant";
                    ServerLogger::logError("Failed to delete points from %s: %s", 
                                           db_type.c_str(), delete_result.error_message.c_str());
                    
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
                "documents" : collection_name;
            
            std::vector<std::string> all_ids;
            std::string offset = "";
            const int batch_size = 1000;
            
            ServerLogger::logDebug("Starting to list documents from collection '%s'", 
                                   effective_collection_name.c_str());
            
            // Use scroll API to get all points
            do {
                auto result_future = pImpl->vector_db_->scrollPoints(effective_collection_name, batch_size, offset);
                VectorResult result = result_future.get();
                
                if (!result.success)
                {
                    std::string db_type = (pImpl->config_.vectorDatabase == DatabaseConfig::VectorDatabase::FAISS) ? "FAISS" : "Qdrant";
                    throw std::runtime_error("Failed to scroll points in " + db_type + ": " + result.error_message);
                }
                
                // Process results from response_data (works for both FAISS and Qdrant)  
                bool hasPoints = false;
                if (result.response_data.contains("result"))
                {
                    nlohmann::json resultData = result.response_data["result"];
                    
                    // Handle different response formats
                    nlohmann::json points;
                    if (resultData.contains("points") && resultData["points"].is_array())
                    {
                        // Qdrant format
                        points = resultData["points"];
                    }
                    else if (resultData.is_array())
                    {
                        // FAISS format (direct array)
                        points = resultData;
                    }
                    
                    if (!points.empty())
                    {
                        hasPoints = true;
                        
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
                    }
                }
                
                if (!hasPoints)
                {
                    break; // No more points
                }
                
                // Update offset for next batch
                // For FAISS, this may be handled differently than Qdrant
                if (result.response_data.contains("next_page_offset") &&
                    !result.response_data["next_page_offset"].is_null())
                {
                    if (result.response_data["next_page_offset"].is_string())
                    {
                        offset = result.response_data["next_page_offset"].get<std::string>();
                    }
                    else if (result.response_data["next_page_offset"].is_number())
                    {
                        offset = std::to_string(result.response_data["next_page_offset"].get<int64_t>());
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
                "documents" : collection_name;
            
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
                
                auto result_future = pImpl->vector_db_->getPoints(effective_collection_name, batch_ids);
                VectorResult result = result_future.get();
                
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
                
                // Process results from response_data (works for both FAISS and Qdrant)
                std::set<std::string> found_ids;
                
                if (result.response_data.contains("result") && result.response_data["result"].is_array())
                {
                    auto points = result.response_data["result"];
                    
                    for (const auto& point : points)
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

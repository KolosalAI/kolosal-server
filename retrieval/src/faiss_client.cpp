#include "kolosal/faiss_client.hpp"
#include "kolosal/logger.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <sstream>
#include <cmath>

#ifdef USE_FAISS
#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>
#include <faiss/impl/io.h>
#ifdef FAISS_ENABLE_GPU
#include <faiss/gpu/GpuIndexFlat.h>
#include <faiss/gpu/GpuIndexIVFFlat.h>
#include <faiss/gpu/StandardGpuResources.h>
#endif
#endif

namespace kolosal
{

class FaissClient::Impl
{
public:
#ifdef USE_FAISS
    faiss::Index* index_;
    std::unordered_map<std::string, std::string> id_to_internal_id_;
    std::unordered_map<faiss::idx_t, std::string> internal_id_to_id_;
    std::unordered_map<std::string, std::unordered_map<std::string, nlohmann::json>> payloads_;
    std::atomic<faiss::idx_t> next_internal_id_{0};
#ifdef FAISS_ENABLE_GPU
    std::unique_ptr<faiss::gpu::StandardGpuResources> gpu_resources_;
#endif
#endif
    
    Config config_;
    std::string current_collection_;
    std::mutex mutex_;
    bool initialized_;
    
    Impl(const Config& config) 
        : config_(config), initialized_(false)
#ifdef USE_FAISS
        , index_(nullptr)
#endif
    {
        ServerLogger::logInfo("FaissClient initialized with index path: %s", config_.indexPath.c_str());
    }
    
    ~Impl()
    {
#ifdef USE_FAISS
        if (index_)
        {
            delete index_;
        }
#endif
    }
    
    std::future<FaissResult> initializeIndex(const std::string& collection_name, int dimensions)
    {
        return std::async(std::launch::async, [this, collection_name, dimensions]() -> FaissResult {
            std::lock_guard<std::mutex> lock(mutex_);
            
            FaissResult result;
            result.success = false;
            
#ifndef USE_FAISS
            result.error_message = "FAISS support not compiled in";
            return result;
#else
            try
            {
                current_collection_ = collection_name;
                
                // Create index directory if it doesn't exist
                std::filesystem::path index_dir = config_.indexPath;
                if (!std::filesystem::exists(index_dir))
                {
                    std::filesystem::create_directories(index_dir);
                }
                
                std::filesystem::path index_file = index_dir / (collection_name + ".faiss");
                std::filesystem::path metadata_file = index_dir / (collection_name + "_metadata.json");
                
                // Try to load existing index
                if (std::filesystem::exists(index_file))
                {
                    index_ = faiss::read_index(index_file.string().c_str());
                    // Ensure the index supports add_with_ids by wrapping with ID map if needed
                    if (dynamic_cast<faiss::IndexIDMap*>(index_) == nullptr &&
                        dynamic_cast<faiss::IndexIDMap2*>(index_) == nullptr)
                    {
                        index_ = new faiss::IndexIDMap2(index_);
                    }
                    
                    // Load metadata
                    if (std::filesystem::exists(metadata_file))
                    {
                        std::ifstream metadata_stream(metadata_file);
                        nlohmann::json metadata;
                        metadata_stream >> metadata;
                        
                        // Restore ID mappings
                        if (metadata.contains("id_mappings"))
                        {
                            for (const auto& item : metadata["id_mappings"].items())
                            {
                                std::string external_id = item.key();
                                faiss::idx_t internal_id = item.value().get<faiss::idx_t>();
                                id_to_internal_id_[external_id] = std::to_string(internal_id);
                                internal_id_to_id_[internal_id] = external_id;
                            }
                        }
                        
                        // Restore payloads
                        if (metadata.contains("payloads"))
                        {
                            for (const auto& item : metadata["payloads"].items())
                            {
                                std::string external_id = item.key();
                                payloads_[external_id] = item.value();
                            }
                        }
                        
                        if (metadata.contains("next_id"))
                        {
                            next_internal_id_ = metadata["next_id"].get<faiss::idx_t>();
                        }
                    }
                    
                    ServerLogger::logInfo("Loaded existing FAISS index: %s", index_file.string().c_str());
                }
                else
                {
                    // Create new index
                    if (config_.indexType == "Flat")
                    {
                        if (config_.metricType == "L2")
                        {
                            index_ = new faiss::IndexFlatL2(dimensions);
                        }
                        else // IP (Inner Product)
                        {
                            index_ = new faiss::IndexFlatIP(dimensions);
                        }
                        // Wrap with ID map to support add_with_ids
                        index_ = new faiss::IndexIDMap2(index_);
                    }
                    else if (config_.indexType == "IVF")
                    {
                        faiss::Index* quantizer;
                        if (config_.metricType == "L2")
                        {
                            quantizer = new faiss::IndexFlatL2(dimensions);
                            index_ = new faiss::IndexIVFFlat(quantizer, dimensions, config_.nlist);
                        }
                        else
                        {
                            quantizer = new faiss::IndexFlatIP(dimensions);
                            index_ = new faiss::IndexIVFFlat(quantizer, dimensions, config_.nlist, faiss::METRIC_INNER_PRODUCT);
                        }
                        // Wrap with ID map to support add_with_ids
                        index_ = new faiss::IndexIDMap2(index_);
                    }
                    else
                    {
                        result.error_message = "Unsupported index type: " + config_.indexType;
                        return result;
                    }
                    
                    ServerLogger::logInfo("Created new FAISS index: %s (%s, %s)", 
                                          config_.indexType.c_str(), config_.metricType.c_str(), index_file.string().c_str());
                }
                
                initialized_ = true;
                result.success = true;
            }
            catch (const std::exception& ex)
            {
                result.error_message = "Failed to initialize FAISS index: " + std::string(ex.what());
                ServerLogger::logError("FAISS initialization error: %s", ex.what());
            }
            
            return result;
#endif
        });
    }
    
    std::future<FaissResult> saveIndex()
    {
        return std::async(std::launch::async, [this]() -> FaissResult {
            std::lock_guard<std::mutex> lock(mutex_);
            
            FaissResult result;
            result.success = false;
            
#ifndef USE_FAISS
            result.error_message = "FAISS support not compiled in";
            return result;
#else            
            try
            {
                if (!index_ || current_collection_.empty())
                {
                    result.error_message = "No index to save";
                    return result;
                }
                
                std::filesystem::path index_dir = config_.indexPath;
                std::filesystem::path index_file = index_dir / (current_collection_ + ".faiss");
                std::filesystem::path metadata_file = index_dir / (current_collection_ + "_metadata.json");
                
                // Save index
                faiss::write_index(index_, index_file.string().c_str());
                
                // Save metadata
                nlohmann::json metadata;
                metadata["next_id"] = next_internal_id_.load();
                
                // Save ID mappings
                nlohmann::json id_mappings;
                for (const auto& item : id_to_internal_id_)
                {
                    faiss::idx_t internal_id = std::stoll(item.second);
                    id_mappings[item.first] = internal_id;
                }
                metadata["id_mappings"] = id_mappings;
                
                // Save payloads
                metadata["payloads"] = payloads_;
                
                std::ofstream metadata_stream(metadata_file);
                metadata_stream << metadata.dump(2);
                
                result.success = true;
                ServerLogger::logDebug("Saved FAISS index and metadata");
            }
            catch (const std::exception& ex)
            {
                result.error_message = "Failed to save FAISS index: " + std::string(ex.what());
                ServerLogger::logError("FAISS save error: %s", ex.what());
            }
            
            return result;
#endif
        });
    }
};

// FaissClient implementations
FaissClient::FaissClient(const Config& config)
    : pImpl(std::make_unique<Impl>(config))
{
}

FaissClient::~FaissClient() = default;

FaissClient::FaissClient(FaissClient&&) noexcept = default;
FaissClient& FaissClient::operator=(FaissClient&&) noexcept = default;

std::future<FaissResult> FaissClient::testConnection()
{
    return std::async(std::launch::async, []() -> FaissResult {
        FaissResult result;
        result.success = true;
        return result;
    });
}

std::future<FaissResult> FaissClient::createCollection(
    const std::string& collection_name,
    int vector_size,
    const std::string& distance)
{
    return pImpl->initializeIndex(collection_name, vector_size);
}

std::future<FaissResult> FaissClient::collectionExists(const std::string& collection_name)
{
    return std::async(std::launch::async, [this, collection_name]() -> FaissResult {
        FaissResult result;
        
        std::filesystem::path index_dir = pImpl->config_.indexPath;
        std::filesystem::path index_file = index_dir / (collection_name + ".faiss");
        
        result.success = std::filesystem::exists(index_file);
        return result;
    });
}

std::future<FaissResult> FaissClient::upsertPoints(
    const std::string& collection_name,
    const std::vector<FaissPoint>& points)
{
    return std::async(std::launch::async, [this, collection_name, points]() -> FaissResult {
    std::unique_lock<std::mutex> lock(pImpl->mutex_);
        
        FaissResult result;
        result.success = false;
        
#ifndef USE_FAISS
        result.error_message = "FAISS support not compiled in";
        return result;
#else
        try
        {
            if (!pImpl->initialized_ || !pImpl->index_)
            {
                // Attempt lazy initialization using vector dimension from first point
                if (!points.empty()) {
                    int dims = static_cast<int>(points.front().vector.size());
                    ServerLogger::logWarning("FAISS index not initialized. Attempting lazy initialization for collection '%s' with dimension %d", collection_name.c_str(), dims);
                    // Release lock before calling initializeIndex to avoid deadlock
                    lock.unlock();
                    auto init_res = pImpl->initializeIndex(collection_name, dims).get();
                    // Reacquire lock for subsequent modifications
                    lock.lock();
                    if (!init_res.success) {
                        result.error_message = "Lazy initialization failed: " + init_res.error_message;
                        return result;
                    }
                } else {
                    result.error_message = "Index not initialized and no points provided to infer dimensions";
                    return result;
                }
            }
            
            if (points.empty())
            {
                result.success = true;
                return result;
            }
            
            std::vector<float> vectors;
            std::vector<faiss::idx_t> ids;
            
            for (const auto& point : points)
            {
                // Check if point already exists
                faiss::idx_t internal_id;
                auto it = pImpl->id_to_internal_id_.find(point.id);
                if (it != pImpl->id_to_internal_id_.end())
                {
                    internal_id = std::stoll(it->second);
                }
                else
                {
                    internal_id = pImpl->next_internal_id_++;
                    pImpl->id_to_internal_id_[point.id] = std::to_string(internal_id);
                    pImpl->internal_id_to_id_[internal_id] = point.id;
                }
                
                // Store payload
                pImpl->payloads_[point.id] = point.payload;
                
                // Prepare vector data
                std::vector<float> normalized_vector = point.vector;
                if (pImpl->config_.normalizeVectors && pImpl->config_.metricType == "IP")
                {
                    // Normalize vector for cosine similarity using inner product
                    float norm = 0.0f;
                    for (float val : normalized_vector)
                    {
                        norm += val * val;
                    }
                    norm = std::sqrt(norm);
                    if (norm > 0)
                    {
                        for (float& val : normalized_vector)
                        {
                            val /= norm;
                        }
                    }
                }
                
                for (float val : normalized_vector)
                {
                    vectors.push_back(val);
                }
                ids.push_back(internal_id);
                
                result.successful_ids.push_back(point.id);
            }
            
            // Add vectors to index
            pImpl->index_->add_with_ids(points.size(), vectors.data(), ids.data());
            
            // Save index after updates (release lock to avoid deadlock)
            lock.unlock();
            auto save_result = pImpl->saveIndex().get();
            // no need to re-lock; we are done modifying shared state
            if (!save_result.success)
            {
                ServerLogger::logWarning("Failed to save index after upsert: %s", save_result.error_message.c_str());
            }
            
            result.success = true;
            ServerLogger::logInfo("Added %zu points to FAISS index", points.size());
        }
        catch (const std::exception& ex)
        {
            result.error_message = "Failed to upsert points: " + std::string(ex.what());
            ServerLogger::logError("FAISS upsert error: %s", ex.what());
        }
        
        return result;
#endif
    });
}

std::future<FaissResult> FaissClient::deletePoints(
    const std::string& collection_name,
    const std::vector<std::string>& point_ids)
{
    return std::async(std::launch::async, [this, collection_name, point_ids]() -> FaissResult {
    std::unique_lock<std::mutex> lock(pImpl->mutex_);
        
        FaissResult result;
        result.success = false;
        
#ifndef USE_FAISS
        result.error_message = "FAISS support not compiled in";
        return result;
#else        
        try
        {
            if (!pImpl->initialized_ || !pImpl->index_)
            {
                result.error_message = "Index not initialized";
                return result;
            }
            
            std::vector<faiss::idx_t> ids_to_remove;
            
            for (const std::string& point_id : point_ids)
            {
                auto it = pImpl->id_to_internal_id_.find(point_id);
                if (it != pImpl->id_to_internal_id_.end())
                {
                    faiss::idx_t internal_id = std::stoll(it->second);
                    ids_to_remove.push_back(internal_id);
                    
                    // Clean up mappings
                    pImpl->internal_id_to_id_.erase(internal_id);
                    pImpl->id_to_internal_id_.erase(it);
                    pImpl->payloads_.erase(point_id);
                    
                    result.successful_ids.push_back(point_id);
                }
                else
                {
                    result.failed_ids.push_back(point_id);
                }
            }
            
            // Remove from index
            if (!ids_to_remove.empty())
            {
                pImpl->index_->remove_ids(faiss::IDSelectorArray(ids_to_remove.size(), ids_to_remove.data()));
                
                // Save index after deletion (release lock to avoid deadlock)
                lock.unlock();
                auto save_result = pImpl->saveIndex().get();
                // no need to re-lock; function is exiting
                if (!save_result.success)
                {
                    ServerLogger::logWarning("Failed to save index after delete: %s", save_result.error_message.c_str());
                }
            }
            
            result.success = true;
            ServerLogger::logInfo("Removed %zu points from FAISS index", ids_to_remove.size());
        }
        catch (const std::exception& ex)
        {
            result.error_message = "Failed to delete points: " + std::string(ex.what());
            ServerLogger::logError("FAISS delete error: %s", ex.what());
        }
        
        return result;
#endif
    });
}

std::future<FaissResult> FaissClient::getPoints(
    const std::string& collection_name,
    const std::vector<std::string>& point_ids)
{
    return std::async(std::launch::async, [this, collection_name, point_ids]() -> FaissResult {
        std::lock_guard<std::mutex> lock(pImpl->mutex_);
        
        FaissResult result;
        result.success = false;
        
#ifndef USE_FAISS
        result.error_message = "FAISS support not compiled in";
        return result;
#else        
        try
        {
            // Return an array in result field for parity with Qdrant expectations in DocumentService
            nlohmann::json response_points = nlohmann::json::array();
            for (const std::string& point_id : point_ids)
            {
                auto payload_it = pImpl->payloads_.find(point_id);
                if (payload_it != pImpl->payloads_.end())
                {
                    nlohmann::json point;
                    point["id"] = point_id;
                    point["payload"] = payload_it->second;
                    response_points.push_back(point);
                }
            }
            result.response_data["result"] = response_points;
            result.success = true;
        }
        catch (const std::exception& ex)
        {
            result.error_message = "Failed to get points: " + std::string(ex.what());
            ServerLogger::logError("FAISS getPoints error: %s", ex.what());
        }
        
        return result;
#endif
    });
}

std::future<FaissResult> FaissClient::search(
    const std::string& collection_name,
    const std::vector<float>& query_vector,
    int limit,
    float score_threshold)
{
    return std::async(std::launch::async, [this, collection_name, query_vector, limit, score_threshold]() -> FaissResult {
        std::unique_lock<std::mutex> lock(pImpl->mutex_);
        
        FaissResult result;
        result.success = false;
        
#ifndef USE_FAISS
        result.error_message = "FAISS support not compiled in";
        return result;
#else        
        try
        {
            if (!pImpl->initialized_ || !pImpl->index_)
            {
                // Try lazy load from disk
                ServerLogger::logWarning("FAISS search requested but index not initialized. Attempting to load existing index '%s'", collection_name.c_str());
                // Release lock to avoid deadlock
                lock.unlock();
                auto init_res = pImpl->initializeIndex(collection_name, static_cast<int>(query_vector.size())).get();
                // Reacquire lock to continue safely
                lock.lock();
                if (!init_res.success || !pImpl->index_) {
                    result.error_message = "Index not initialized and lazy load failed";
                    return result;
                }
            }
            
            // Normalize query vector if needed
            std::vector<float> normalized_query = query_vector;
            if (pImpl->config_.normalizeVectors && pImpl->config_.metricType == "IP")
            {
                float norm = 0.0f;
                for (float val : normalized_query)
                {
                    norm += val * val;
                }
                norm = std::sqrt(norm);
                if (norm > 0)
                {
                    for (float& val : normalized_query)
                    {
                        val /= norm;
                    }
                }
            }
            
            // Perform search
            std::vector<faiss::idx_t> internal_ids(limit);
            std::vector<float> distances(limit);
            
            pImpl->index_->search(1, normalized_query.data(), limit, distances.data(), internal_ids.data());
            
            // Convert results to JSON format
            nlohmann::json search_results = nlohmann::json::array();
            
            for (int i = 0; i < limit; ++i)
            {
                if (internal_ids[i] < 0) break; // No more results
                
                float score = distances[i];
                
                // Convert distance to similarity score based on metric type
                if (pImpl->config_.metricType == "L2")
                {
                    score = 1.0f / (1.0f + score); // Convert L2 distance to similarity
                }
                // For IP, the score is already similarity (higher is better)
                
                if (score < score_threshold) continue;
                
                auto it = pImpl->internal_id_to_id_.find(internal_ids[i]);
                if (it != pImpl->internal_id_to_id_.end())
                {
                    std::string external_id = it->second;
                    
                    nlohmann::json search_result;
                    search_result["id"] = external_id;
                    search_result["score"] = score;
                    
                    // Add payload if available
                    auto payload_it = pImpl->payloads_.find(external_id);
                    if (payload_it != pImpl->payloads_.end())
                    {
                        search_result["payload"] = payload_it->second;
                    }
                    
                    search_results.push_back(search_result);
                }
            }
            
            result.response_data["result"] = search_results;
            result.success = true;
            
            ServerLogger::logDebug("FAISS search found %zu results", search_results.size());
        }
        catch (const std::exception& ex)
        {
            result.error_message = "Failed to search: " + std::string(ex.what());
            ServerLogger::logError("FAISS search error: %s", ex.what());
        }
        
        return result;
#endif
    });
}

std::future<FaissResult> FaissClient::scrollPoints(
    const std::string& collection_name,
    int limit,
    const std::string& offset)
{
    return std::async(std::launch::async, [this, collection_name, limit, offset]() -> FaissResult {
        std::lock_guard<std::mutex> lock(pImpl->mutex_);
        
        FaissResult result;
        result.success = false;
        
#ifndef USE_FAISS
        result.error_message = "FAISS support not compiled in";
        return result;
#else        
        try
        {
            nlohmann::json points = nlohmann::json::array();
            size_t start_idx = 0;
            if (!offset.empty())
            {
                start_idx = std::stoull(offset);
            }
            size_t count = 0;
            size_t current_idx = 0;
            for (const auto& item : pImpl->payloads_)
            {
                if (current_idx >= start_idx && count < static_cast<size_t>(limit))
                {
                    nlohmann::json point;
                    point["id"] = item.first;
                    point["payload"] = item.second;
                    points.push_back(point);
                    count++;
                }
                current_idx++;
            }
            // Provide array directly for consistency with getPoints/search format
            result.response_data["result"] = points;
            if (current_idx > start_idx + limit)
            {
                result.response_data["next_page_offset"] = std::to_string(start_idx + limit);
            }
            result.success = true;
        }
        catch (const std::exception& ex)
        {
            result.error_message = "Failed to scroll points: " + std::string(ex.what());
            ServerLogger::logError("FAISS scrollPoints error: %s", ex.what());
        }
        
        return result;
#endif
    });
}

} // namespace kolosal

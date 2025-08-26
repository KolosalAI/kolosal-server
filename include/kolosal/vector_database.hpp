#pragma once

#include "kolosal/export.hpp"
#include "kolosal/qdrant_client.hpp"
#ifdef USE_FAISS
#include "kolosal/faiss_client.hpp"
#endif
#include <memory>
#include <future>
#include <string>
#include <vector>
#include <unordered_map>
#include <json.hpp>

namespace kolosal
{

/**
 * @brief Generic point structure for vector database operations
 */
struct VectorPoint
{
    std::string id;
    std::vector<float> vector;
    std::unordered_map<std::string, nlohmann::json> payload;
    
    // Convert to Qdrant format
    QdrantPoint toQdrantPoint() const
    {
        QdrantPoint qpoint;
        qpoint.id = id;
        qpoint.vector = vector;
        qpoint.payload = payload;
        return qpoint;
    }
    
#ifdef USE_FAISS
    // Convert to FAISS format
    FaissPoint toFaissPoint() const
    {
        FaissPoint fpoint;
        fpoint.id = id;
        fpoint.vector = vector;
        fpoint.payload = payload;
        return fpoint;
    }
#endif
    
    // Create from Qdrant result
    static VectorPoint fromQdrantResult(const nlohmann::json& result)
    {
        VectorPoint point;
        if (result.contains("id"))
        {
            point.id = result["id"].is_string() ? 
                result["id"].get<std::string>() : 
                std::to_string(result["id"].get<int64_t>());
        }
        if (result.contains("payload"))
        {
            point.payload = result["payload"];
        }
        if (result.contains("vector"))
        {
            point.vector = result["vector"].get<std::vector<float>>();
        }
        return point;
    }
};

/**
 * @brief Generic result structure for vector database operations
 */
struct VectorResult
{
    bool success = false;
    std::string error_message;
    int status_code = 200;
    
    std::vector<std::string> successful_ids;
    std::vector<std::string> failed_ids;
    
    nlohmann::json response_data;
    
    // Create from Qdrant result
    static VectorResult fromQdrantResult(const QdrantResult& qresult)
    {
        VectorResult result;
        result.success = qresult.success;
        result.error_message = qresult.error_message;
        result.status_code = qresult.status_code;
        result.successful_ids = qresult.successful_ids;
        result.failed_ids = qresult.failed_ids;
        result.response_data = qresult.response_data;
        return result;
    }
    
#ifdef USE_FAISS    
    // Create from FAISS result
    static VectorResult fromFaissResult(const FaissResult& fresult)
    {
        VectorResult result;
        result.success = fresult.success;
        result.error_message = fresult.error_message;
        result.status_code = fresult.status_code;
        result.successful_ids = fresult.successful_ids;
        result.failed_ids = fresult.failed_ids;
        result.response_data = fresult.response_data;
        return result;
    }
#endif
};

/**
 * @brief Abstract vector database client interface
 */
class KOLOSAL_SERVER_API IVectorDatabase
{
public:
    virtual ~IVectorDatabase() = default;
    
    virtual std::future<VectorResult> testConnection() = 0;
    virtual std::future<VectorResult> createCollection(const std::string& collection_name, int vector_size, const std::string& distance = "Cosine") = 0;
    virtual std::future<VectorResult> collectionExists(const std::string& collection_name) = 0;
    virtual std::future<VectorResult> upsertPoints(const std::string& collection_name, const std::vector<VectorPoint>& points) = 0;
    virtual std::future<VectorResult> deletePoints(const std::string& collection_name, const std::vector<std::string>& point_ids) = 0;
    virtual std::future<VectorResult> getPoints(const std::string& collection_name, const std::vector<std::string>& point_ids) = 0;
    virtual std::future<VectorResult> search(const std::string& collection_name, const std::vector<float>& query_vector, int limit = 10, float score_threshold = 0.0f) = 0;
    virtual std::future<VectorResult> scrollPoints(const std::string& collection_name, int limit = 1000, const std::string& offset = "") = 0;
};

/**
 * @brief Qdrant implementation of vector database interface
 */
class KOLOSAL_SERVER_API QdrantVectorDatabase : public IVectorDatabase
{
private:
    std::unique_ptr<QdrantClient> client_;
    
public:
    explicit QdrantVectorDatabase(const QdrantClient::Config& config)
        : client_(std::make_unique<QdrantClient>(config))
    {
    }
    
    std::future<VectorResult> testConnection() override
    {
        return std::async(std::launch::async, [this]() {
            auto result = client_->testConnection().get();
            return VectorResult::fromQdrantResult(result);
        });
    }
    
    std::future<VectorResult> createCollection(const std::string& collection_name, int vector_size, const std::string& distance) override
    {
        return std::async(std::launch::async, [this, collection_name, vector_size, distance]() {
            auto result = client_->createCollection(collection_name, vector_size, distance).get();
            return VectorResult::fromQdrantResult(result);
        });
    }
    
    std::future<VectorResult> collectionExists(const std::string& collection_name) override
    {
        return std::async(std::launch::async, [this, collection_name]() {
            auto result = client_->collectionExists(collection_name).get();
            return VectorResult::fromQdrantResult(result);
        });
    }
    
    std::future<VectorResult> upsertPoints(const std::string& collection_name, const std::vector<VectorPoint>& points) override
    {
        return std::async(std::launch::async, [this, collection_name, points]() {
            std::vector<QdrantPoint> qpoints;
            for (const auto& point : points)
            {
                qpoints.push_back(point.toQdrantPoint());
            }
            auto result = client_->upsertPoints(collection_name, qpoints).get();
            return VectorResult::fromQdrantResult(result);
        });
    }
    
    std::future<VectorResult> deletePoints(const std::string& collection_name, const std::vector<std::string>& point_ids) override
    {
        return std::async(std::launch::async, [this, collection_name, point_ids]() {
            auto result = client_->deletePoints(collection_name, point_ids).get();
            return VectorResult::fromQdrantResult(result);
        });
    }
    
    std::future<VectorResult> getPoints(const std::string& collection_name, const std::vector<std::string>& point_ids) override
    {
        return std::async(std::launch::async, [this, collection_name, point_ids]() {
            auto result = client_->getPoints(collection_name, point_ids).get();
            return VectorResult::fromQdrantResult(result);
        });
    }
    
    std::future<VectorResult> search(const std::string& collection_name, const std::vector<float>& query_vector, int limit, float score_threshold) override
    {
        return std::async(std::launch::async, [this, collection_name, query_vector, limit, score_threshold]() {
            auto result = client_->search(collection_name, query_vector, limit, score_threshold).get();
            return VectorResult::fromQdrantResult(result);
        });
    }
    
    std::future<VectorResult> scrollPoints(const std::string& collection_name, int limit, const std::string& offset) override
    {
        return std::async(std::launch::async, [this, collection_name, limit, offset]() {
            auto result = client_->scrollPoints(collection_name, limit, offset).get();
            return VectorResult::fromQdrantResult(result);
        });
    }
};

#ifdef USE_FAISS
/**
 * @brief FAISS implementation of vector database interface
 */
class KOLOSAL_SERVER_API FaissVectorDatabase : public IVectorDatabase
{
private:
    std::unique_ptr<FaissClient> client_;
    
public:
    explicit FaissVectorDatabase(const FaissClient::Config& config)
        : client_(std::make_unique<FaissClient>(config))
    {
    }
    
    std::future<VectorResult> testConnection() override
    {
        return std::async(std::launch::async, [this]() {
            auto result = client_->testConnection().get();
            return VectorResult::fromFaissResult(result);
        });
    }
    
    std::future<VectorResult> createCollection(const std::string& collection_name, int vector_size, const std::string& distance) override
    {
        return std::async(std::launch::async, [this, collection_name, vector_size, distance]() {
            auto result = client_->createCollection(collection_name, vector_size, distance).get();
            return VectorResult::fromFaissResult(result);
        });
    }
    
    std::future<VectorResult> collectionExists(const std::string& collection_name) override
    {
        return std::async(std::launch::async, [this, collection_name]() {
            auto result = client_->collectionExists(collection_name).get();
            return VectorResult::fromFaissResult(result);
        });
    }
    
    std::future<VectorResult> upsertPoints(const std::string& collection_name, const std::vector<VectorPoint>& points) override
    {
        return std::async(std::launch::async, [this, collection_name, points]() {
            std::vector<FaissPoint> fpoints;
            for (const auto& point : points)
            {
                fpoints.push_back(point.toFaissPoint());
            }
            auto result = client_->upsertPoints(collection_name, fpoints).get();
            return VectorResult::fromFaissResult(result);
        });
    }
    
    std::future<VectorResult> deletePoints(const std::string& collection_name, const std::vector<std::string>& point_ids) override
    {
        return std::async(std::launch::async, [this, collection_name, point_ids]() {
            auto result = client_->deletePoints(collection_name, point_ids).get();
            return VectorResult::fromFaissResult(result);
        });
    }
    
    std::future<VectorResult> getPoints(const std::string& collection_name, const std::vector<std::string>& point_ids) override
    {
        return std::async(std::launch::async, [this, collection_name, point_ids]() {
            auto result = client_->getPoints(collection_name, point_ids).get();
            return VectorResult::fromFaissResult(result);
        });
    }
    
    std::future<VectorResult> search(const std::string& collection_name, const std::vector<float>& query_vector, int limit, float score_threshold) override
    {
        return std::async(std::launch::async, [this, collection_name, query_vector, limit, score_threshold]() {
            auto result = client_->search(collection_name, query_vector, limit, score_threshold).get();
            return VectorResult::fromFaissResult(result);
        });
    }
    
    std::future<VectorResult> scrollPoints(const std::string& collection_name, int limit, const std::string& offset) override
    {
        return std::async(std::launch::async, [this, collection_name, limit, offset]() {
            auto result = client_->scrollPoints(collection_name, limit, offset).get();
            return VectorResult::fromFaissResult(result);
        });
    }
};
#endif

/**
 * @brief Factory for creating vector database instances
 */
class KOLOSAL_SERVER_API VectorDatabaseFactory
{
public:
    enum class DatabaseType
    {
        QDRANT,
        FAISS
    };
    
    static std::unique_ptr<IVectorDatabase> create(DatabaseType type, const nlohmann::json& config)
    {
        switch (type)
        {
            case DatabaseType::QDRANT:
            {
                QdrantClient::Config qconfig;
                if (config.contains("host")) qconfig.host = config["host"];
                if (config.contains("port")) qconfig.port = config["port"];
                if (config.contains("apiKey")) qconfig.apiKey = config["apiKey"];
                if (config.contains("timeout")) qconfig.timeout = config["timeout"];
                if (config.contains("maxConnections")) qconfig.maxConnections = config["maxConnections"];
                if (config.contains("connectionTimeout")) qconfig.connectionTimeout = config["connectionTimeout"];
                
                return std::make_unique<QdrantVectorDatabase>(qconfig);
            }
            
#ifdef USE_FAISS
            case DatabaseType::FAISS:
            {
                FaissClient::Config fconfig;
                if (config.contains("indexType")) fconfig.indexType = config["indexType"];
                if (config.contains("indexPath")) fconfig.indexPath = config["indexPath"];
                if (config.contains("dimensions")) fconfig.dimensions = config["dimensions"];
                if (config.contains("normalizeVectors")) fconfig.normalizeVectors = config["normalizeVectors"];
                if (config.contains("nlist")) fconfig.nlist = config["nlist"];
                if (config.contains("nprobe")) fconfig.nprobe = config["nprobe"];
                if (config.contains("useGPU")) fconfig.useGPU = config["useGPU"];
                if (config.contains("gpuDevice")) fconfig.gpuDevice = config["gpuDevice"];
                if (config.contains("metricType")) fconfig.metricType = config["metricType"];
                
                return std::make_unique<FaissVectorDatabase>(fconfig);
            }
#endif
            
            default:
                throw std::runtime_error("Unsupported vector database type");
        }
    }
};

} // namespace kolosal

#pragma once

#include "export.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <future>
#include <memory>
#include <json.hpp>

namespace kolosal
{

/**
 * @brief Point to be indexed in Qdrant
 */
struct QdrantPoint
{
    std::string id;
    std::vector<float> vector;
    std::unordered_map<std::string, nlohmann::json> payload;
    
    nlohmann::json to_json() const;
};

/**
 * @brief Result of Qdrant operations
 */
struct QdrantResult
{
    bool success = false;
    std::string error_message;
    std::string operation_id;
    int status_code = 0;
    
    // For upsert operations
    std::vector<std::string> successful_ids;
    std::vector<std::string> failed_ids;
};

/**
 * @brief Async Qdrant client for vector database operations
 * 
 * This client provides thread-safe, async operations for interacting with Qdrant
 * vector database. All operations return futures for non-blocking execution.
 */
class KOLOSAL_SERVER_API QdrantClient
{
public:
    /**
     * @brief Configuration for Qdrant client
     */
    struct Config
    {
        std::string host = "localhost";
        int port = 6333;
        std::string apiKey = "";
        int timeout = 30;
        int maxConnections = 10;
        int connectionTimeout = 5;
        bool useHttps = false;
    };
    
    /**
     * @brief Constructor
     * @param config Client configuration
     */
    explicit QdrantClient(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~QdrantClient();
    
    // Delete copy constructor and assignment operator
    QdrantClient(const QdrantClient&) = delete;
    QdrantClient& operator=(const QdrantClient&) = delete;
    
    // Move constructor and assignment operator
    QdrantClient(QdrantClient&&) noexcept;
    QdrantClient& operator=(QdrantClient&&) noexcept;
    
    /**
     * @brief Test connection to Qdrant server
     * @return Future with result of connection test
     */
    std::future<QdrantResult> testConnection();
    
    /**
     * @brief Create a collection if it doesn't exist
     * @param collection_name Name of the collection
     * @param vector_size Size of the vectors (embedding dimensions)
     * @param distance Distance metric (Cosine, Euclid, Dot)
     * @return Future with result of collection creation
     */
    std::future<QdrantResult> createCollection(
        const std::string& collection_name,
        int vector_size,
        const std::string& distance = "Cosine"
    );
    
    /**
     * @brief Check if collection exists
     * @param collection_name Name of the collection
     * @return Future with result (success=true if collection exists)
     */
    std::future<QdrantResult> collectionExists(const std::string& collection_name);
    
    /**
     * @brief Upsert points (insert or update) into collection
     * @param collection_name Name of the collection
     * @param points Vector of points to upsert
     * @return Future with result of upsert operation
     */
    std::future<QdrantResult> upsertPoints(
        const std::string& collection_name,
        const std::vector<QdrantPoint>& points
    );
    
    /**
     * @brief Delete points from collection
     * @param collection_name Name of the collection
     * @param point_ids Vector of point IDs to delete
     * @return Future with result of delete operation
     */
    std::future<QdrantResult> deletePoints(
        const std::string& collection_name,
        const std::vector<std::string>& point_ids
    );
    
    /**
     * @brief Search for similar vectors
     * @param collection_name Name of the collection
     * @param query_vector Vector to search for
     * @param limit Maximum number of results
     * @param score_threshold Minimum score threshold
     * @return Future with search results
     */
    std::future<QdrantResult> search(
        const std::string& collection_name,
        const std::vector<float>& query_vector,
        int limit = 10,
        float score_threshold = 0.0f
    );

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace kolosal

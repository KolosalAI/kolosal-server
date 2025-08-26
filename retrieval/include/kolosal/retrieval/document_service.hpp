#pragma once

#include "../export.hpp"
#include "add_document_types.hpp"
#include "retrieve_types.hpp"
#include "remove_document_types.hpp"
#include "../qdrant_client.hpp"
#include "../server_config.hpp"
#include <memory>
#include <future>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <json.hpp>

namespace kolosal
{
namespace retrieval
{

/**
 * @brief Service for managing document operations
 * 
 * This service handles document indexing, embedding generation, and vector storage.
 * It provides thread-safe, async operations for document management.
 */
class KOLOSAL_SERVER_API DocumentService
{
public:
    /**
     * @brief Constructor
     * @param database_config Database configuration
     */
    explicit DocumentService(const DatabaseConfig& database_config);
    
    /**
     * @brief Destructor
     */
    ~DocumentService();
    
    // Delete copy constructor and assignment operator
    DocumentService(const DocumentService&) = delete;
    DocumentService& operator=(const DocumentService&) = delete;
    
    // Move constructor and assignment operator
    DocumentService(DocumentService&&) noexcept;
    DocumentService& operator=(DocumentService&&) noexcept;
    
    /**
     * @brief Initialize the service (test connections, create collections)
     * @return Future with result of initialization
     */
    std::future<bool> initialize();
    
    /**
     * @brief Add documents to the vector database
     * @param request Documents to add
     * @return Future with response containing results
     */
    std::future<AddDocumentsResponse> addDocuments(const AddDocumentsRequest& request);
    
    /**
     * @brief Remove documents from the vector database
     * @param request Document IDs to remove
     * @return Future with response containing results
     */
    std::future<RemoveDocumentsResponse> removeDocuments(const RemoveDocumentsRequest& request);
    
    /**
     * @brief Test database connection
     * @return Future with connection test result
     */
    std::future<bool> testConnection();
    
    /**
     * @brief Get embedding for text using configured model
     * @param text Text to embed
     * @param model_id Model ID to use (optional, uses default if empty)
     * @return Future with embedding vector
     */
    std::future<std::vector<float>> getEmbedding(const std::string& text, const std::string& model_id = "");
    
    /**
     * @brief Retrieve similar documents using vector search
     * @param request Retrieve request with query and parameters
     * @return Future with retrieve response containing similar documents
     */
    std::future<RetrieveResponse> retrieveDocuments(const RetrieveRequest& request);
    
    /**
     * @brief List all document IDs in the collection
     * @param collection_name Collection name (optional, uses default if empty)
     * @return Future with list of document IDs
     */
    std::future<std::vector<std::string>> listDocuments(const std::string& collection_name = "");
    
    /**
     * @brief Get full document information by IDs
     * @param ids Document IDs to retrieve
     * @param collection_name Collection name (optional, uses default if empty)
     * @return Future with document information response
     */
    std::future<std::vector<std::pair<std::string, std::optional<std::pair<std::string, std::unordered_map<std::string, nlohmann::json>>>>>> getDocumentsInfo(const std::vector<std::string>& ids, const std::string& collection_name = "");

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace retrieval
} // namespace kolosal

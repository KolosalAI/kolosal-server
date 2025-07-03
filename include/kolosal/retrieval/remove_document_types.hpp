#pragma once

#include "../export.hpp"
#include <json.hpp>
#include <string>
#include <vector>

namespace kolosal
{
namespace retrieval
{

/**
 * @brief Request data type for remove_documents endpoint
 * 
 * Contains the list of document IDs to be removed from the vector database.
 * Collection name is always "documents".
 */
struct KOLOSAL_SERVER_API RemoveDocumentsRequest
{
    std::vector<std::string> ids; // Document IDs to remove
    std::string collection_name; // Always set to "documents"
    
    /**
     * @brief Populates request from JSON
     * @param j JSON object to parse (expects "document_ids" field)
     */
    void from_json(const nlohmann::json& j);
    
    /**
     * @brief Validates the request
     * @return true if valid, false otherwise
     */
    bool validate() const;
};

/**
 * @brief Result for a single document removal operation
 * 
 * Contains the document ID and the status of the removal operation.
 */
struct DocumentRemovalResult
{
    std::string id;
    std::string status; // "removed", "failed", "not_found"
    
    /**
     * @brief Converts result to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Response data type for remove_documents endpoint
 * 
 * Contains the results of document removal operations.
 */
struct KOLOSAL_SERVER_API RemoveDocumentsResponse
{
    std::vector<DocumentRemovalResult> results;
    std::string collection_name;
    int removed_count = 0;
    int failed_count = 0;
    int not_found_count = 0;
    
    /**
     * @brief Add a successful removal result
     * @param document_id ID of the removed document
     */
    void addRemoved(const std::string& document_id);
    
    /**
     * @brief Add a failed removal result
     * @param document_id ID of the document that failed to be removed
     */
    void addFailed(const std::string& document_id);
    
    /**
     * @brief Add a not found result
     * @param document_id ID of the document that was not found
     */
    void addNotFound(const std::string& document_id);
    
    /**
     * @brief Converts response to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
    
    /**
     * @brief Validates the response
     * @return true if valid, false otherwise
     */
    bool validate() const;
};

/**
 * @brief Error response for remove_documents endpoint
 * 
 * Contains error information when document removal fails.
 */
struct KOLOSAL_SERVER_API RemoveDocumentsErrorResponse
{
    std::string error;
    std::string error_type;
    std::string param;
    std::string code;
    
    /**
     * @brief Converts error response to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

} // namespace retrieval
} // namespace kolosal

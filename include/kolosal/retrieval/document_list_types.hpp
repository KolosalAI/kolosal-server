#pragma once

#include "../export.hpp"
#include <json.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace kolosal
{
namespace retrieval
{

/**
 * @brief Response data type for list_documents endpoint
 * 
 * Contains a list of document IDs in the collection.
 */
struct KOLOSAL_SERVER_API ListDocumentsResponse
{
    std::vector<std::string> document_ids;
    int total_count = 0;
    std::string collection_name;
    
    /**
     * @brief Converts response to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Document information with full details
 * 
 * Represents a document with ID, text content and metadata.
 */
struct KOLOSAL_SERVER_API DocumentInfo
{
    std::string id;
    std::string text;
    std::unordered_map<std::string, nlohmann::json> metadata;
    
    /**
     * @brief Converts document info to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Request data type for documents_info endpoint
 * 
 * Contains the document IDs to retrieve information for.
 */
struct KOLOSAL_SERVER_API DocumentsInfoRequest
{
    std::vector<std::string> ids;
    
    /**
     * @brief Populates request from JSON
     * @param j JSON object to parse
     */
    void from_json(const nlohmann::json& j);
    
    /**
     * @brief Validates the request
     * @return true if valid, false otherwise
     */
    bool validate() const;
};

/**
 * @brief Response data type for documents_info endpoint
 * 
 * Contains the full information for requested documents.
 */
struct KOLOSAL_SERVER_API DocumentsInfoResponse
{
    std::vector<DocumentInfo> documents;
    int found_count = 0;
    int not_found_count = 0;
    std::vector<std::string> not_found_ids;
    std::string collection_name;
    
    /**
     * @brief Converts response to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Error response data type for document endpoints
 * 
 * Represents structured error information when document operations fail.
 */
struct KOLOSAL_SERVER_API DocumentsErrorResponse
{
    std::string error;
    std::string error_type = "invalid_request_error";
    std::string param = "";
    std::string code = "";
    
    /**
     * @brief Converts error response to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

} // namespace retrieval
} // namespace kolosal

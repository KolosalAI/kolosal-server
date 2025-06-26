#pragma once

#include "../export.hpp"
#include <json.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace kolosal
{

/**
 * @brief Single document for indexing
 */
struct Document
{
    std::string text;
    std::unordered_map<std::string, nlohmann::json> metadata;
    
    /**
     * @brief Converts document to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
    
    /**
     * @brief Populates document from JSON
     * @param j JSON object to parse
     */
    void from_json(const nlohmann::json& j);
    
    /**
     * @brief Validates the document
     * @return true if valid, false otherwise
     */
    bool validate() const;
};

/**
 * @brief Request model for add_documents endpoint
 */
struct KOLOSAL_SERVER_API AddDocumentsRequest
{
    std::vector<Document> documents;
    std::string collection_name = ""; // Optional, uses default if empty
    
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
 * @brief Single document indexing result
 */
struct DocumentResult
{
    std::string id;
    bool success = false;
    std::string error = "";
    
    /**
     * @brief Converts result to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Response model for add_documents endpoint
 */
struct KOLOSAL_SERVER_API AddDocumentsResponse
{
    std::vector<DocumentResult> results;
    int successful_count = 0;
    int failed_count = 0;
    std::string collection_name;
    
    /**
     * @brief Adds a successful document result
     * @param document_id ID of the indexed document
     */
    void addSuccess(const std::string& document_id);
    
    /**
     * @brief Adds a failed document result
     * @param error Error message
     */
    void addFailure(const std::string& error);
    
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
 * @brief Error response for add_documents endpoint
 */
struct KOLOSAL_SERVER_API AddDocumentsErrorResponse
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

} // namespace kolosal

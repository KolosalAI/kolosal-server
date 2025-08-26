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
 * @brief Retrieve request data type for vector search endpoint
 * 
 * Contains the query and parameters for vector similarity search.
 */
struct KOLOSAL_SERVER_API RetrieveRequest
{
    std::string query;
    int k = 10;  // Number of top similar documents to return
    std::string collection_name = "";  // Optional, uses default if empty
    float score_threshold = 0.0f;  // Minimum similarity score threshold
    
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
 * @brief Single retrieved document result
 * 
 * Represents a document found during vector similarity search.
 */
struct RetrievedDocument
{
    std::string id;
    std::string text;
    std::unordered_map<std::string, nlohmann::json> metadata;
    float score = 0.0f;  // Similarity score
    
    /**
     * @brief Converts document to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Response data type for retrieve endpoint
 * 
 * Contains the results of vector similarity search.
 */
struct KOLOSAL_SERVER_API RetrieveResponse
{
    std::vector<RetrievedDocument> documents;
    std::string query;
    int k = 10;
    std::string collection_name;
    float score_threshold = 0.0f;
    int total_found = 0;
    
    /**
     * @brief Adds a retrieved document result
     * @param document Retrieved document
     */
    void addDocument(const RetrievedDocument& document);
    
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
 * @brief Error response data type for retrieve endpoint
 * 
 * Represents structured error information when retrieval fails.
 */
struct KOLOSAL_SERVER_API RetrieveErrorResponse
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

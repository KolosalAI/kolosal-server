#include "kolosal/routes/vector_search_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include <json.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

namespace kolosal::routes
{

VectorSearchRoute::VectorSearchRoute()
{
    ServerLogger::logInfo("VectorSearchRoute initialized");
}

VectorSearchRoute::~VectorSearchRoute() = default;

bool VectorSearchRoute::match(const std::string& method, const std::string& path)
{
    return method == "POST" && (path == "/vector-search" || path == "/api/v1/vector-search");
}

void VectorSearchRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        // Only handle POST requests for vector search
        handleVectorSearch(sock, body);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in VectorSearchRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void VectorSearchRoute::handleVectorSearch(SocketType sock, const std::string& body)
{
    try
    {
        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        json request_data;
        try
        {
            request_data = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Validate required fields
        if (!request_data.contains("vector") && !request_data.contains("query"))
        {
            sendErrorResponse(sock, 400, "Either 'vector' or 'query' is required", "missing_parameter", "vector|query");
            return;
        }

        int limit = request_data.value("limit", 10);
        double threshold = request_data.value("threshold", 0.0);
        std::string collection = request_data.value("collection", "default");

        // Enhanced parameter validation
        if (limit <= 0 || limit > 1000) {
            sendErrorResponse(sock, 400, "limit must be between 1 and 1000 (got " + std::to_string(limit) + ")", "invalid_parameter", "limit");
            return;
        }
        
        if (threshold < 0.0 || threshold > 1.0) {
            sendErrorResponse(sock, 400, "threshold must be between 0.0 and 1.0 (got " + std::to_string(threshold) + ")", "invalid_parameter", "threshold");
            return;
        }
        
        if (collection.empty()) {
            sendErrorResponse(sock, 400, "collection name cannot be empty", "invalid_parameter", "collection");
            return;
        }
        
        // Validate collection name format (alphanumeric and underscores only)
        if (!std::all_of(collection.begin(), collection.end(), [](char c) {
            return std::isalnum(c) || c == '_' || c == '-';
        })) {
            sendErrorResponse(sock, 400, "collection name must contain only alphanumeric characters, underscores, and hyphens", "invalid_parameter", "collection");
            return;
        }

        if (request_data.contains("vector"))
        {
            // Direct vector search
            if (!request_data["vector"].is_array()) {
                sendErrorResponse(sock, 400, "Vector must be an array of numbers", "invalid_parameter", "vector");
                return;
            }
            
            std::vector<float> query_vector;
            try {
                for (const auto& val : request_data["vector"]) {
                    if (val.is_number()) {
                        float float_val = val.get<float>();
                        // Check for invalid float values
                        if (std::isnan(float_val) || std::isinf(float_val)) {
                            sendErrorResponse(sock, 400, "Vector elements cannot be NaN or infinite", "invalid_parameter", "vector");
                            return;
                        }
                        query_vector.push_back(float_val);
                    } else {
                        sendErrorResponse(sock, 400, "All vector elements must be numbers", "invalid_parameter", "vector");
                        return;
                    }
                }
            } catch (const std::exception& e) {
                sendErrorResponse(sock, 400, "Invalid vector format: " + std::string(e.what()), "invalid_parameter", "vector");
                return;
            }
            
            if (query_vector.empty()) {
                sendErrorResponse(sock, 400, "Vector cannot be empty", "invalid_parameter", "vector");
                return;
            }
            
            // Validate vector dimension (common embedding dimensions)
            size_t vector_size = query_vector.size();
            if (vector_size > 4096) {
                sendErrorResponse(sock, 400, "Vector dimension too large (max 4096, got " + std::to_string(vector_size) + ")", "invalid_parameter", "vector");
                return;
            }
            
            ServerLogger::logInfo("Vector search request - Vector size: %zu, Limit: %d, Collection: %s", 
                                 query_vector.size(), limit, collection.c_str());

            try
            {
                // Start timing
                auto start_time = std::chrono::high_resolution_clock::now();
                
                auto& serverAPI = ServerAPI::instance();
                auto& documentService = serverAPI.getDocumentService();
                
                // Calculate processing time (even for placeholder implementation)
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                int processing_time_ms = static_cast<int>(duration.count());
                
                ServerLogger::logInfo("Direct vector search processed in %d ms (vector size: %zu)", 
                                     processing_time_ms, query_vector.size());
                
                // Create a dummy query for vector search - this needs to be implemented in the document service
                // For now, return a basic response indicating vector search capability
                json response;
                response["success"] = true;
                response["data"] = {
                    {"search_type", "vector"},
                    {"vector_size", query_vector.size()},
                    {"results", json::array()},
                    {"total_results", 0},
                    {"processing_time_ms", processing_time_ms},
                    {"search_metadata", {
                        {"vector_preparation_time_ms", 0},
                        {"similarity_search_time_ms", 0},
                        {"total_time_ms", processing_time_ms}
                    }},
                    {"message", "Direct vector search with document service not fully implemented yet"}
                };

                sendSuccessResponse(sock, response);
            }
            catch (const std::runtime_error& e)
            {
                // Handle DocumentService specific errors for direct vector search
                std::string error_msg = e.what();
                ServerLogger::logError("DocumentService error in direct vector search: %s", error_msg.c_str());
                
                if (error_msg.find("not initialized") != std::string::npos) {
                    sendErrorResponse(sock, 503, "Vector search service not ready for direct vector queries", "service_not_ready");
                } else if (error_msg.find("Collection") != std::string::npos && error_msg.find("does not exist") != std::string::npos) {
                    sendErrorResponse(sock, 404, "Target collection for vector search does not exist", "collection_not_found", "collection");
                } else if (error_msg.find("dimension") != std::string::npos) {
                    sendErrorResponse(sock, 400, "Vector dimension mismatch with collection schema: " + error_msg, "dimension_mismatch", "vector");
                } else {
                    sendErrorResponse(sock, 500, "Direct vector search service error: " + error_msg, "service_error");
                }
            }
        }
        else if (request_data.contains("query"))
        {
            // Text query to vector search using document service
            std::string query = request_data["query"];
            
            // Enhanced query validation
            if (query.empty()) {
                sendErrorResponse(sock, 400, "Query cannot be empty", "invalid_parameter", "query");
                return;
            }
            
            if (query.length() > 10000) {
                sendErrorResponse(sock, 400, "Query too long (max 10000 characters, got " + std::to_string(query.length()) + ")", "invalid_parameter", "query");
                return;
            }
            
            // Check for potentially malicious content (basic security)
            if (query.find_first_of("<>\"'&") != std::string::npos) {
                ServerLogger::logWarning("Potentially unsafe query received: %s", query.substr(0, 100).c_str());
                // Don't reject but log for monitoring
            }
            
            ServerLogger::logInfo("Vector search request - Query: %s, Limit: %d, Collection: %s", 
                                 query.c_str(), limit, collection.c_str());

            try
            {
                // Start timing
                auto start_time = std::chrono::high_resolution_clock::now();
                
                auto& serverAPI = ServerAPI::instance();
                auto& documentService = serverAPI.getDocumentService();
                
                // Use RetrieveRequest for the search
                kolosal::retrieval::RetrieveRequest retrieveRequest;
                retrieveRequest.query = query;
                retrieveRequest.k = limit;
                retrieveRequest.collection_name = collection;
                
                // Perform the search
                auto future_response = documentService.retrieveDocuments(retrieveRequest);
                auto retrieve_response = future_response.get();
                
                // Calculate processing time
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                int processing_time_ms = static_cast<int>(duration.count());
                
                ServerLogger::logInfo("Vector search completed in %d ms for query: %s", 
                                     processing_time_ms, query.c_str());
                
                // Convert to vector search response format
                json response;
                response["success"] = true;
                response["data"] = {
                    {"search_type", "query"},
                    {"query", query},
                    {"results", json::array()},
                    {"total_results", retrieve_response.total_found},
                    {"processing_time_ms", processing_time_ms},
                    {"search_metadata", {
                        {"embedding_time_ms", 0}, // TODO: Get from document service if available
                        {"vector_search_time_ms", 0}, // TODO: Get from document service if available
                        {"total_time_ms", processing_time_ms}
                    }}
                };
                
                // Convert retrieved documents to vector search result format
                for (const auto& doc : retrieve_response.documents)
                {
                    json result_item = {
                        {"id", doc.id},
                        {"score", doc.score},
                        {"content", doc.text},
                        {"metadata", doc.metadata}
                    };
                    response["data"]["results"].push_back(result_item);
                }

                sendSuccessResponse(sock, response);
            }
            catch (const std::runtime_error& e)
            {
                // Handle DocumentService specific errors with more context
                std::string error_msg = e.what();
                ServerLogger::logError("DocumentService error in vector search: %s", error_msg.c_str());
                
                if (error_msg.find("not initialized") != std::string::npos) {
                    sendErrorResponse(sock, 503, "Vector search service not ready - server may be initializing. Please try again in a moment.", "service_not_ready");
                } else if (error_msg.find("embedding") != std::string::npos) {
                    sendErrorResponse(sock, 500, "Failed to generate embeddings for query: " + error_msg, "embedding_generation_failed");
                } else if (error_msg.find("Collection") != std::string::npos && error_msg.find("does not exist") != std::string::npos) {
                    sendErrorResponse(sock, 404, "The specified collection does not exist. Please verify the collection name or index documents first.", "collection_not_found", "collection");
                } else if (error_msg.find("Vector search failed") != std::string::npos) {
                    sendErrorResponse(sock, 503, "Vector database search failed: " + error_msg + ". Check database connectivity.", "vector_search_failed");
                } else if (error_msg.find("timeout") != std::string::npos) {
                    sendErrorResponse(sock, 408, "Vector search request timed out. The query may be too complex or the system is under heavy load.", "request_timeout");
                } else {
                    sendErrorResponse(sock, 500, "Vector search service error: " + error_msg, "service_error");
                }
            }
        }
    }
    catch (const json::exception& ex)
    {
        ServerLogger::logError("JSON processing error in vector search: %s", ex.what());
        sendErrorResponse(sock, 400, "Invalid request format: " + std::string(ex.what()), "json_processing_error");
    }
    catch (const std::invalid_argument& ex)
    {
        ServerLogger::logError("Invalid argument in vector search: %s", ex.what());
        sendErrorResponse(sock, 400, "Invalid request parameters: " + std::string(ex.what()), "invalid_parameter");
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Unexpected error in vector search: %s", ex.what());
        sendErrorResponse(sock, 500, "An unexpected error occurred during vector search", "internal_server_error");
    }
}

void VectorSearchRoute::handleVectorSearchInfo(SocketType sock)
{
    try
    {
        json response;
        response["success"] = true;
        response["data"] = {
            {"endpoint", "/vector-search"},
            {"description", "Direct vector similarity search endpoint"},
            {"methods", json::array({"GET", "POST"})},
            {"parameters", {
                {"vector", {
                    {"type", "array"},
                    {"required", false},
                    {"description", "Vector array for direct similarity search"}
                }},
                {"query", {
                    {"type", "string"},
                    {"required", false},
                    {"description", "Text query to convert to vector and search"}
                }},
                {"limit", {
                    {"type", "integer"},
                    {"required", false},
                    {"default", 10},
                    {"description", "Maximum number of results to return"}
                }},
                {"threshold", {
                    {"type", "number"},
                    {"required", false},
                    {"default", 0.0},
                    {"description", "Minimum similarity threshold for results"}
                }},
                {"collection", {
                    {"type", "string"},
                    {"required", false},
                    {"default", "default"},
                    {"description", "Collection to search in"}
                }}
            }},
            {"note", "Either 'vector' or 'query' parameter is required"}
        };

        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error getting vector search info: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to get endpoint information");
    }
}

void VectorSearchRoute::sendErrorResponse(SocketType sock, int status, const std::string& message, 
                                        const std::string& error_type, const std::string& param)
{
    json error_response;
    error_response["success"] = false;
    error_response["error"] = {
        {"type", error_type},
        {"message", message},
        {"code", status}
    };
    if (!param.empty())
    {
        error_response["error"]["param"] = param;
    }

    std::string response_body = error_response.dump();
    send_response(sock, status, response_body);
}

void VectorSearchRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::string response_body = data.dump();
    send_response(sock, 200, response_body);
}

} // namespace kolosal::routes

#include "kolosal/routes/vector_search_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>

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
    return (path == "/vector-search" || path == "/api/v1/vector-search") && 
           (method == "GET" || method == "POST");
}

void VectorSearchRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        // For demonstration purposes, we'll handle based on body content
        // In a real implementation, you'd parse the HTTP method from the request
        
        if (body.empty())
        {
            // Assume GET request for endpoint info
            handleVectorSearchInfo(sock);
        }
        else
        {
            // Assume POST request for actual search
            handleVectorSearch(sock, body);
        }
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

        if (request_data.contains("vector"))
        {
            // Direct vector search
            auto vector = request_data["vector"];
            if (!vector.is_array())
            {
                sendErrorResponse(sock, 400, "Vector must be an array of numbers", "invalid_parameter", "vector");
                return;
            }

            ServerLogger::logInfo("Vector search request - Vector size: %zu, Limit: %d, Collection: %s", 
                                 vector.size(), limit, collection.c_str());

            // Simulate vector search
            json response;
            response["success"] = true;
            response["data"] = {
                {"search_type", "vector"},
                {"vector_dimension", vector.size()},
                {"results", json::array({
                    {
                        {"id", "vec_1"},
                        {"score", 0.98},
                        {"vector", json::array({0.1, 0.2, 0.3})},
                        {"metadata", {
                            {"document_id", "doc_1"},
                            {"chunk_id", 0}
                        }}
                    },
                    {
                        {"id", "vec_2"},
                        {"score", 0.89},
                        {"vector", json::array({0.2, 0.3, 0.4})},
                        {"metadata", {
                            {"document_id", "doc_2"},
                            {"chunk_id", 1}
                        }}
                    }
                })},
                {"total_results", 2},
                {"processing_time_ms", 23}
            };

            sendSuccessResponse(sock, response);
        }
        else if (request_data.contains("query"))
        {
            // Text query to vector search
            std::string query = request_data["query"];
            
            ServerLogger::logInfo("Vector search request - Query: %s, Limit: %d, Collection: %s", 
                                 query.c_str(), limit, collection.c_str());

            // Simulate query-based vector search (would involve embedding the query first)
            json response;
            response["success"] = true;
            response["data"] = {
                {"search_type", "query"},
                {"query", query},
                {"results", json::array({
                    {
                        {"id", "vec_3"},
                        {"score", 0.92},
                        {"content", "Content snippet matching the query"},
                        {"metadata", {
                            {"document_id", "doc_3"},
                            {"chunk_id", 2}
                        }}
                    }
                })},
                {"total_results", 1},
                {"processing_time_ms", 67}
            };

            sendSuccessResponse(sock, response);
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in vector search: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to perform vector search");
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

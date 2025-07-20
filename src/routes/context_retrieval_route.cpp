#include "kolosal/routes/context_retrieval_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace kolosal::routes
{

ContextRetrievalRoute::ContextRetrievalRoute()
{
    ServerLogger::logInfo("ContextRetrievalRoute initialized");
}

ContextRetrievalRoute::~ContextRetrievalRoute() = default;

bool ContextRetrievalRoute::match(const std::string& method, const std::string& path)
{
    return (path == "/context-retrieval" || path == "/api/v1/context-retrieval") && 
           (method == "GET" || method == "POST");
}

void ContextRetrievalRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        // For demonstration purposes, we'll handle based on body content
        // In a real implementation, you'd parse the HTTP method from the request
        
        if (body.empty())
        {
            // Assume GET request for endpoint info
            handleContextRetrievalInfo(sock);
        }
        else
        {
            // Assume POST request for actual retrieval
            handleContextRetrieval(sock, body);
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in ContextRetrievalRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void ContextRetrievalRoute::handleContextRetrieval(SocketType sock, const std::string& body)
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
        if (!request_data.contains("query"))
        {
            sendErrorResponse(sock, 400, "Query is required", "missing_parameter", "query");
            return;
        }

        std::string query = request_data["query"];
        int limit = request_data.value("limit", 10);
        double threshold = request_data.value("threshold", 0.7);
        std::string collection = request_data.value("collection", "default");

        ServerLogger::logInfo("Context retrieval request - Query: %s, Limit: %d, Collection: %s", 
                             query.c_str(), limit, collection.c_str());

        // Simulate context retrieval
        json response;
        response["success"] = true;
        response["data"] = {
            {"query", query},
            {"results", json::array({
                {
                    {"id", "doc_1"},
                    {"content", "Sample context content related to the query"},
                    {"score", 0.95},
                    {"metadata", {
                        {"source", "document_1.pdf"},
                        {"page", 1}
                    }}
                },
                {
                    {"id", "doc_2"},
                    {"content", "Another relevant context snippet"},
                    {"score", 0.87},
                    {"metadata", {
                        {"source", "document_2.pdf"},
                        {"page", 3}
                    }}
                }
            })},
            {"total_results", 2},
            {"processing_time_ms", 45}
        };

        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in context retrieval: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to retrieve context");
    }
}

void ContextRetrievalRoute::handleContextRetrievalInfo(SocketType sock)
{
    try
    {
        json response;
        response["success"] = true;
        response["data"] = {
            {"endpoint", "/context-retrieval"},
            {"description", "Semantic search and context retrieval endpoint"},
            {"methods", json::array({"GET", "POST"})},
            {"parameters", {
                {"query", {
                    {"type", "string"},
                    {"required", true},
                    {"description", "Search query for context retrieval"}
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
                    {"default", 0.7},
                    {"description", "Minimum similarity threshold for results"}
                }},
                {"collection", {
                    {"type", "string"},
                    {"required", false},
                    {"default", "default"},
                    {"description", "Collection to search in"}
                }}
            }}
        };

        sendSuccessResponse(sock, response);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error getting context retrieval info: %s", ex.what());
        sendErrorResponse(sock, 500, "Failed to get endpoint information");
    }
}

void ContextRetrievalRoute::sendErrorResponse(SocketType sock, int status, const std::string& message, 
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

void ContextRetrievalRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::string response_body = data.dump();
    send_response(sock, 200, response_body);
}

} // namespace kolosal::routes

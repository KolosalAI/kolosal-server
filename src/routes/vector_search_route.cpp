#include "kolosal/routes/vector_search_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
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
                        query_vector.push_back(val.get<float>());
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
            
            ServerLogger::logInfo("Vector search request - Vector size: %zu, Limit: %d, Collection: %s", 
                                 query_vector.size(), limit, collection.c_str());

            try
            {
                auto& serverAPI = ServerAPI::instance();
                auto& documentService = serverAPI.getDocumentService();
                
                // Create a dummy query for vector search - this needs to be implemented in the document service
                // For now, return a basic response indicating vector search capability
                json response;
                response["success"] = true;
                response["data"] = {
                    {"search_type", "vector"},
                    {"vector_size", query_vector.size()},
                    {"results", json::array()},
                    {"total_results", 0},
                    {"processing_time_ms", 0},
                    {"message", "Direct vector search with document service not fully implemented yet"}
                };

                sendSuccessResponse(sock, response);
            }
            catch (const std::runtime_error& e)
            {
                // DocumentService not available
                json response;
                response["success"] = false;
                response["error"] = "Document service not available: " + std::string(e.what());
                sendSuccessResponse(sock, response);
            }
        }
        else if (request_data.contains("query"))
        {
            // Text query to vector search using document service
            std::string query = request_data["query"];
            
            ServerLogger::logInfo("Vector search request - Query: %s, Limit: %d, Collection: %s", 
                                 query.c_str(), limit, collection.c_str());

            try
            {
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
                
                // Convert to vector search response format
                json response;
                response["success"] = true;
                response["data"] = {
                    {"search_type", "query"},
                    {"query", query},
                    {"results", json::array()},
                    {"total_results", retrieve_response.total_found},
                    {"processing_time_ms", 0} // TODO: add actual timing
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
                // DocumentService not available
                json response;
                response["success"] = false;
                response["error"] = "Document service not available: " + std::string(e.what());
                sendSuccessResponse(sock, response);
            }
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

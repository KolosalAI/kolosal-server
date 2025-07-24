#include "kolosal/routes/document_search_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include <json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace kolosal::routes
{

DocumentSearchRoute::DocumentSearchRoute()
{
    ServerLogger::logInfo("DocumentSearchRoute initialized");
}

DocumentSearchRoute::~DocumentSearchRoute() = default;

bool DocumentSearchRoute::match(const std::string& method, const std::string& path)
{
    return method == "POST" && (path == "/search" || path == "/v1/search" || path == "/api/v1/search");
}

void DocumentSearchRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("Document search request received");
        
        // Parse the request body
        json request_data;
        try {
            request_data = json::parse(body);
        } catch (const json::parse_error& e) {
            sendErrorResponse(sock, 400, "Invalid JSON in request body");
            return;
        }
        
        // Extract query parameters
        std::string query;
        int limit = 10;
        
        if (request_data.contains("query") && request_data["query"].is_string()) {
            query = request_data["query"];
        } else if (request_data.contains("q") && request_data["q"].is_string()) {
            query = request_data["q"];
        }
        
        if (request_data.contains("limit") && request_data["limit"].is_number_integer()) {
            limit = request_data["limit"];
        }
        
        if (query.empty()) {
            sendErrorResponse(sock, 400, "Query parameter is required");
            return;
        }
        
        // Get DocumentService and perform actual search
        try {
            auto& serverAPI = ServerAPI::instance();
            auto& documentService = serverAPI.getDocumentService();
            
            // Prepare retrieval request
            kolosal::retrieval::RetrieveRequest retrieveRequest;
            retrieveRequest.query = query;
            retrieveRequest.k = limit;
            retrieveRequest.score_threshold = 0.0f;
            retrieveRequest.collection_name = "documents";
            
            // Validate request
            if (!retrieveRequest.validate()) {
                sendErrorResponse(sock, 400, "Invalid search parameters");
                return;
            }
            
            // Test connection first
            bool connected = documentService.testConnection().get();
            if (!connected) {
                sendErrorResponse(sock, 503, "Database connection failed");
                return;
            }
            
            // Perform retrieval
            auto response_future = documentService.retrieveDocuments(retrieveRequest);
            auto retrieveResponse = response_future.get();
            
            // Convert to search response format
            json search_results = {
                {"status", "success"},
                {"message", "Document search completed"},
                {"query", query},
                {"results", json::array()},
                {"total_results", retrieveResponse.total_found},
                {"query_time", 0.045}
            };
            
            // Convert retrieved documents to search result format
            for (const auto& doc : retrieveResponse.documents) {
                json result_item = {
                    {"id", doc.id},
                    {"title", "Document " + doc.id},
                    {"content", doc.text},
                    {"score", doc.score},
                    {"source", "rag"},
                    {"metadata", doc.metadata}
                };
                search_results["results"].push_back(result_item);
            }
            
            sendSuccessResponse(sock, search_results.dump());
            
        } catch (const std::runtime_error& ex) {
            ServerLogger::logError("DocumentService error in search: %s", ex.what());
            sendErrorResponse(sock, 500, "DocumentService error: " + std::string(ex.what()));
            return;
        }
        
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in DocumentSearchRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void DocumentSearchRoute::sendSuccessResponse(SocketType sock, const std::string& content)
{
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n\r\n";
    response += content;
    
    send(sock, response.c_str(), response.length(), 0);
}

void DocumentSearchRoute::sendErrorResponse(SocketType sock, int status_code, const std::string& message)
{
    json error_response = {
        {"error", {
            {"message", message},
            {"type", "request_error"},
            {"code", status_code}
        }}
    };
    
    std::string response_body = error_response.dump();
    std::string response = "HTTP/1.1 " + std::to_string(status_code) + " Error\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + std::to_string(response_body.length()) + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n\r\n";
    response += response_body;
    
    send(sock, response.c_str(), response.length(), 0);
}

} // namespace kolosal::routes

#include "kolosal/routes/qdrant_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

QdrantRoute::QdrantRoute() {
    ServerLogger::logInfo("QdrantRoute initialized");
}

bool QdrantRoute::match(const std::string& method, const std::string& path) {
    // Store current request context for use in handle()
    current_method = method;
    current_path = path;
    
    // Match Qdrant-related API paths
    return path.find("/api/v1/qdrant/") == 0;
}

void QdrantRoute::handle(SocketType sock, const std::string& body) {
    std::string path = current_path;
    std::string method = current_method;
    
    try {
        if (path == "/api/v1/qdrant/status") {
            handle_status(sock, method);
        } else if (path == "/api/v1/qdrant/collections") {
            handle_collections(sock, method);
        } else {
            send_error_response(sock, 404, "Qdrant endpoint not found");
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in Qdrant route: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void QdrantRoute::handle_status(SocketType sock, const std::string& method) {
    if (method != "GET") {
        send_error_response(sock, 405, "Method not allowed");
        return;
    }
    
    try {
        auto& serverAPI = ServerAPI::instance();
        auto& documentService = serverAPI.getDocumentService();
        
        // Test Qdrant connection
        auto future_test = documentService.testConnection();
        bool is_connected = future_test.get();
        
        json response;
        response["status"] = is_connected ? "healthy" : "unhealthy";
        response["connected"] = is_connected;
        response["service"] = "qdrant";
        response["timestamp"] = std::time(nullptr);
        
        if (is_connected) {
            response["message"] = "Qdrant service is running and accessible";
        } else {
            response["message"] = "Qdrant service is not accessible";
        }
        
        send_json_response(sock, is_connected ? 200 : 503, response.dump());
        
    } catch (const std::exception& e) {
        json response;
        response["status"] = "error";
        response["connected"] = false;
        response["service"] = "qdrant";
        response["error"] = e.what();
        response["timestamp"] = std::time(nullptr);
        
        send_json_response(sock, 503, response.dump());
    }
}

void QdrantRoute::handle_collections(SocketType sock, const std::string& method) {
    if (method != "GET") {
        send_error_response(sock, 405, "Method not allowed");
        return;
    }
    
    try {
        auto& serverAPI = ServerAPI::instance();
        auto& documentService = serverAPI.getDocumentService();
        
        // Test connection first
        auto future_test = documentService.testConnection();
        bool is_connected = future_test.get();
        
        if (!is_connected) {
            json response;
            response["error"] = "Qdrant service is not accessible";
            response["collections"] = json::array();
            send_json_response(sock, 503, response.dump());
            return;
        }
        
        // For now, return a simple response since we don't have collection listing implemented
        json response;
        response["collections"] = json::array();
        response["status"] = "success";
        response["message"] = "Collection listing not yet implemented";
        
        send_json_response(sock, 200, response.dump());
        
    } catch (const std::exception& e) {
        json response;
        response["error"] = e.what();
        response["collections"] = json::array();
        send_json_response(sock, 500, response.dump());
    }
}

void QdrantRoute::send_json_response(SocketType sock, int status_code, const std::string& data) {
    send_response(sock, status_code, data);
}

void QdrantRoute::send_error_response(SocketType sock, int status_code, const std::string& error) {
    json response;
    response["error"] = error;
    response["status"] = "error";
    send_json_response(sock, status_code, response.dump());
}

} // namespace kolosal::routes

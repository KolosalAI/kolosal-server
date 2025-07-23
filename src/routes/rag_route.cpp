#include "kolosal/routes/rag_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

class RAGChatRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        return (method == "POST" && (path == "/v1/rag/chat" || path == "/rag/chat" || path == "/api/v1/rag/chat"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("RAG Chat request received");
            
            if (body.empty()) {
                json error_response = {
                    {"error", {
                        {"message", "Request body is required"},
                        {"type", "invalid_request_error"},
                        {"code", 400}
                    }}
                };
                send_response(sock, 400, error_response.dump());
                return;
            }
            
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                json error_response = {
                    {"error", {
                        {"message", "Invalid JSON in request body"},
                        {"type", "invalid_request_error"},
                        {"code", 400}
                    }}
                };
                send_response(sock, 400, error_response.dump());
                return;
            }
            
            // For now, return a basic response indicating the feature is available but not fully implemented
            json response = {
                {"status", "success"},
                {"message", "RAG chat endpoint is available"},
                {"query", request_data.value("query", "")},
                {"response", "RAG functionality is being processed. This endpoint is operational but requires full agent system initialization."},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"rag_enabled", true}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in RAG chat route: %s", e.what());
            json error_response = {
                {"error", {
                    {"message", "Internal server error"},
                    {"type", "server_error"},
                    {"code", 500}
                }}
            };
            send_response(sock, 500, error_response.dump());
        }
    }
};

class RAGSearchRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        return (method == "POST" && (path == "/v1/rag/search" || path == "/rag/search" || path == "/api/v1/rag/search"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("RAG Search request received");
            
            if (body.empty()) {
                json error_response = {
                    {"error", {
                        {"message", "Request body is required"},
                        {"type", "invalid_request_error"},
                        {"code", 400}
                    }}
                };
                send_response(sock, 400, error_response.dump());
                return;
            }
            
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                json error_response = {
                    {"error", {
                        {"message", "Invalid JSON in request body"},
                        {"type", "invalid_request_error"},
                        {"code", 400}
                    }}
                };
                send_response(sock, 400, error_response.dump());
                return;
            }
            
            // For now, return a basic response indicating the feature is available
            json response = {
                {"status", "success"},
                {"message", "RAG search endpoint is available"},
                {"query", request_data.value("query", "")},
                {"results", json::array()},
                {"total_results", 0},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"search_enabled", true}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in RAG search route: %s", e.what());
            json error_response = {
                {"error", {
                    {"message", "Internal server error"},
                    {"type", "server_error"},
                    {"code", 500}
                }}
            };
            send_response(sock, 500, error_response.dump());
        }
    }
};

RAGRoute::RAGRoute() {
    // This route will be set up with sub-routes
}

bool RAGRoute::match(const std::string& method, const std::string& path) {
    // This should not be called directly
    return false;
}

void RAGRoute::handle(SocketType sock, const std::string& body) {
    // This should not be called directly
}

void RAGRoute::setup_routes(Server& server) {
    ServerLogger::logInfo("Setting up RAG routes");
    
    // Add individual RAG routes
    server.addRoute(std::make_unique<RAGChatRoute>());
    server.addRoute(std::make_unique<RAGSearchRoute>());
    
    ServerLogger::logInfo("RAG routes setup completed");
}

} // namespace kolosal::routes

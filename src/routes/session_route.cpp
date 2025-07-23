#include "kolosal/routes/session_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

class SessionCreateRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        return (method == "POST" && (path == "/v1/sessions" || path == "/sessions" || path == "/api/v1/sessions"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Session Create request received");
            
            // Generate a mock session ID
            std::string session_id = "session_" + std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            
            json response = {
                {"status", "success"},
                {"message", "Session created successfully"},
                {"session_id", session_id},
                {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"expires_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() + 3600000}, // 1 hour
                {"session_enabled", true}
            };
            
            send_response(sock, 201, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in session create route: %s", e.what());
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

class SessionListRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        return (method == "GET" && (path == "/v1/sessions" || path == "/sessions" || path == "/api/v1/sessions"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Session List request received");
            
            json response = {
                {"status", "success"},
                {"message", "Sessions retrieved successfully"},
                {"sessions", json::array()}, // Empty for now
                {"total_count", 0},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in session list route: %s", e.what());
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

class SessionGetRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        std::regex pattern("/v1/sessions/([^/]+)|/sessions/([^/]+)|/api/v1/sessions/([^/]+)");
        return method == "GET" && std::regex_match(path, pattern);
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Session Get request received");
            
            json response = {
                {"status", "success"},
                {"message", "Session retrieved successfully"},
                {"session_id", "unknown"},
                {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() - 1800000}, // 30 min ago
                {"expires_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() + 1800000}, // 30 min from now
                {"active", true}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in session get route: %s", e.what());
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

class SessionHistoryRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        std::regex pattern("/v1/sessions/([^/]+)/history|/sessions/([^/]+)/history|/api/v1/sessions/([^/]+)/history");
        return method == "GET" && std::regex_match(path, pattern);
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Session History request received");
            
            json response = {
                {"status", "success"},
                {"message", "Chat history retrieved successfully"},
                {"session_id", "test-session"},
                {"messages", json::array({
                    {
                        {"id", "msg-1"},
                        {"role", "user"},
                        {"content", "Hello, how are you?"},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count() - 300000}
                    },
                    {
                        {"id", "msg-2"},
                        {"role", "assistant"},
                        {"content", "Hello! I'm doing well, thank you for asking. How can I help you today?"},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count() - 290000}
                    }
                })},
                {"total_messages", 2}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in session history route: %s", e.what());
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

SessionRoute::SessionRoute() {
    // This route will be set up with sub-routes
}

bool SessionRoute::match(const std::string& method, const std::string& path) {
    // This should not be called directly
    return false;
}

void SessionRoute::handle(SocketType sock, const std::string& body) {
    // This should not be called directly
}

void SessionRoute::setup_routes(Server& server) {
    ServerLogger::logInfo("Setting up Session routes");
    
    // Add individual session routes
    server.addRoute(std::make_unique<SessionCreateRoute>());
    server.addRoute(std::make_unique<SessionListRoute>());
    server.addRoute(std::make_unique<SessionGetRoute>());
    server.addRoute(std::make_unique<SessionHistoryRoute>());
    
    ServerLogger::logInfo("Session routes setup completed");
}

} // namespace kolosal::routes

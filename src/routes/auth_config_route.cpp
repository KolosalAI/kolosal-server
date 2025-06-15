#include "kolosal/routes/auth_config_route.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/auth/auth_middleware.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <stdexcept>

using json = nlohmann::json;

namespace kolosal {

AuthConfigRoute::AuthConfigRoute() {
    ServerLogger::logInfo("Auth config route initialized");
}

bool AuthConfigRoute::match(const std::string& method, const std::string& path) {
    return (path.find("/v1/auth") == 0) &&
           ((method == "GET" && (path == "/v1/auth/config" || path == "/v1/auth/stats")) ||
            (method == "PUT" && path == "/v1/auth/config") ||
            (method == "POST" && path == "/v1/auth/clear"));
}

void AuthConfigRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Parse the request to get method and path
        // Note: In a real implementation, we'd extract this from the HTTP request
        // For now, we'll determine based on the body content and handle accordingly
        
        if (body.empty()) {
            // GET request - determine if it's config or stats based on the path
            // This is a simplified approach - in practice you'd parse the full HTTP request
            handleGetConfig(sock);
        } else {
            json j = json::parse(body);
            
            if (j.contains("action")) {
                std::string action = j["action"];
                if (action == "get_config") {
                    handleGetConfig(sock);
                } else if (action == "update_config") {
                    handleUpdateConfig(sock, body);
                } else if (action == "get_stats") {
                    handleGetStats(sock);
                } else if (action == "clear_rate_limit") {
                    handleClearRateLimit(sock, body);
                } else {
                    json error = {
                        {"error", {
                            {"message", "Unknown action: " + action},
                            {"type", "invalid_request_error"}
                        }}
                    };
                    send_response(sock, 400, error.dump());
                }
            } else {
                // Assume it's a config update
                handleUpdateConfig(sock, body);
            }
        }
        
    } catch (const json::exception& ex) {
        ServerLogger::logError("JSON parsing error in auth config route: %s", ex.what());
        json error = {
            {"error", {
                {"message", "Invalid JSON in request body"},
                {"type", "invalid_request_error"}
            }}
        };
        send_response(sock, 400, error.dump());
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error in auth config route: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Internal error: ") + ex.what()},
                {"type", "server_error"}
            }}
        };
        send_response(sock, 500, error.dump());
    }
}

void AuthConfigRoute::handleGetConfig(SocketType sock) {
    try {
        auto& authMiddleware = ServerAPI::instance().getAuthMiddleware();
        auto& rateLimiter = authMiddleware.getRateLimiter();
        auto& corsHandler = authMiddleware.getCorsHandler();
        
        auto rateLimiterConfig = rateLimiter.getConfig();
        auto corsConfig = corsHandler.getConfig();
          json response = {
            {"rate_limiter", {
                {"enabled", rateLimiterConfig.enabled},
                {"max_requests", rateLimiterConfig.maxRequests},
                {"window_size", rateLimiterConfig.windowSize.count()}
            }},
            {"cors", {
                {"enabled", corsConfig.enabled},
                {"allowed_origins", corsConfig.allowedOrigins},
                {"allowed_methods", corsConfig.allowedMethods},
                {"allowed_headers", corsConfig.allowedHeaders},
                {"allow_credentials", corsConfig.allowCredentials},
                {"max_age", corsConfig.maxAge}
            }}
        };
        
        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("Sent auth config response");
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error getting auth config: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Failed to get config: ") + ex.what()},
                {"type", "server_error"}
            }}        };
        send_response(sock, 500, error.dump());
    }
}

void AuthConfigRoute::handleUpdateConfig(SocketType sock, const std::string& body) {
    try {
        json j = json::parse(body);
        auto& authMiddleware = ServerAPI::instance().getAuthMiddleware();
        
        // Validate and update rate limiter config
        if (j.contains("rate_limiter")) {
            auto& rl = j["rate_limiter"];
            if (rl.contains("max_requests") && !rl["max_requests"].is_number_unsigned()) {
                json error = {
                    {"error", {
                        {"message", "max_requests must be a positive integer"},
                        {"type", "invalid_request_error"}
                    }}
                };
                send_response(sock, 400, error.dump());
                return;
            }
              if (rl.contains("window_size") && !rl["window_size"].is_number_unsigned()) {
                json error = {
                    {"error", {
                        {"message", "window_size must be a positive integer"},
                        {"type", "invalid_request_error"}
                    }}
                };
                send_response(sock, 400, error.dump());
                return;
            }
            
            // Update rate limiter configuration
            auto& rateLimiter = authMiddleware.getRateLimiter();
            auto config = rateLimiter.getConfig();
            
            if (rl.contains("enabled")) config.enabled = rl["enabled"];
            if (rl.contains("max_requests")) config.maxRequests = rl["max_requests"];
            if (rl.contains("window_size")) config.windowSize = std::chrono::seconds(rl["window_size"]);
            
            rateLimiter.updateConfig(config);
        }        
        // Validate CORS config
        if (j.contains("cors")) {
            auto& cors = j["cors"];
            if (cors.contains("allowed_origins") && !cors["allowed_origins"].is_array()) {
                json error = {
                    {"error", {
                        {"message", "allowed_origins must be an array"},
                        {"type", "invalid_request_error"}
                    }}
                };
                send_response(sock, 400, error.dump());
                return;
            }
            
            if (cors.contains("allowed_methods") && !cors["allowed_methods"].is_array()) {
                json error = {
                    {"error", {
                        {"message", "allowed_methods must be an array"},
                        {"type", "invalid_request_error"}
                    }}
                };
                send_response(sock, 400, error.dump());
                return;
            }
            
            // Update CORS configuration
            auto& corsHandler = authMiddleware.getCorsHandler();
            auto config = corsHandler.getConfig();
            
            if (cors.contains("enabled")) config.enabled = cors["enabled"];
            if (cors.contains("allowed_origins")) {
                config.allowedOrigins.clear();
                for (const auto& origin : cors["allowed_origins"]) {
                    config.allowedOrigins.push_back(origin);
                }
            }
            if (cors.contains("allowed_methods")) {
                config.allowedMethods.clear();
                for (const auto& method : cors["allowed_methods"]) {
                    config.allowedMethods.push_back(method);
                }
            }
            if (cors.contains("allowed_headers")) {
                config.allowedHeaders.clear();
                for (const auto& header : cors["allowed_headers"]) {
                    config.allowedHeaders.push_back(header);
                }
            }
            if (cors.contains("allow_credentials")) config.allowCredentials = cors["allow_credentials"];
            if (cors.contains("max_age")) config.maxAge = cors["max_age"];
            
            corsHandler.updateConfig(config);
        }
        
        json response = {
            {"message", "Authentication configuration updated successfully"},
            {"status", "success"}
        };
        
        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("Updated auth configuration");
        
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error updating auth config: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Failed to update config: ") + ex.what()},
                {"type", "server_error"}
            }}
        };
        send_response(sock, 500, error.dump());
    }
}

void AuthConfigRoute::handleGetStats(SocketType sock) {
    try {
        auto& authMiddleware = ServerAPI::instance().getAuthMiddleware();        auto& rateLimiter = authMiddleware.getRateLimiter();
        
        auto stats = rateLimiter.getStatistics();
        
        json clientsJson = json::object();
        for (const auto& client : stats) {
            clientsJson[client.first] = {
                {"request_count", client.second}
            };
        }
        
        json response = {
            {"rate_limit_stats", {
                {"total_clients", stats.size()},
                {"total_requests", 0}, // Would need to sum up all request counts
                {"clients", clientsJson}
            }},
            {"cors_stats", {
                {"message", "CORS statistics not implemented yet"}
            }}
        };
        
        send_response(sock, 200, response.dump());
        ServerLogger::logInfo("Served auth statistics");
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error getting auth stats: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Failed to get stats: ") + ex.what()},
                {"type", "server_error"}
            }}
        };
        send_response(sock, 500, error.dump());
    }
}

void AuthConfigRoute::handleClearRateLimit(SocketType sock, const std::string& body) {
    try {
        json j = json::parse(body);
        auto& authMiddleware = ServerAPI::instance().getAuthMiddleware();
        auto& rateLimiter = authMiddleware.getRateLimiter();
        
        if (j.contains("client_ip")) {
            std::string clientIP = j["client_ip"];
            rateLimiter.clearClient(clientIP);
            
            json response = {
                {"message", "Rate limit data cleared for client: " + clientIP},
                {"status", "success"}
            };
            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("Cleared rate limit data for client: %s", clientIP.c_str());
        } else if (j.contains("clear_all") && j["clear_all"].is_boolean() && j["clear_all"]) {
            rateLimiter.clearAll();
            
            json response = {
                {"message", "All rate limit data cleared"},
                {"status", "success"}
            };
            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("Cleared all rate limit data");
        } else {
            json error = {
                {"error", {
                    {"message", "Must specify either client_ip or clear_all=true"},
                    {"type", "invalid_request_error"}
                }}
            };
            send_response(sock, 400, error.dump());
        }
    } catch (const std::exception& ex) {
        ServerLogger::logError("Error clearing rate limit data: %s", ex.what());
        json error = {
            {"error", {
                {"message", std::string("Failed to clear data: ") + ex.what()},
                {"type", "server_error"}
            }}
        };
        send_response(sock, 500, error.dump());
    }
}

} // namespace kolosal

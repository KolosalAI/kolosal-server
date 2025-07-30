#include "kolosal/routes/agents/agents_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <regex>
#include <iostream>

using json = nlohmann::json;

namespace kolosal::routes::agents {

AgentsRoute::AgentsRoute() {
    ServerLogger::logInfo("AgentsRoute initialized");
}

bool AgentsRoute::match(const std::string& method, const std::string& path) {
    // Match agent management endpoints
    if ((method == "GET" || method == "POST") && path == "/api/v1/agents") {
        current_method_ = method;
        current_path_ = path;
        return true;
    }
    
    // Match agent-specific endpoints with regex
    std::regex agent_pattern("^/api/v1/agents/([^/]+)(?:/(start|stop|status))?$");
    std::smatch matches;
    
    if (std::regex_match(path, matches, agent_pattern)) {
        if ((method == "GET" || method == "PUT" || method == "DELETE" || method == "POST")) {
            current_method_ = method;
            current_path_ = path;
            return true;
        }
    }
    
    // Handle OPTIONS for CORS
    if (method == "OPTIONS" && (path.find("/api/v1/agents") == 0)) {
        current_method_ = method;
        current_path_ = path;
        return true;
    }
    
    return false;
}

void AgentsRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Handle OPTIONS requests for CORS
        if (current_method_ == "OPTIONS") {
            std::map<std::string, std::string> headers = {
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
                {"Access-Control-Max-Age", "86400"}
            };
            send_response(sock, 200, "");
            return;
        }
        
        // Route to appropriate handler based on path and method
        if (current_path_ == "/api/v1/agents" && current_method_ == "GET") {
            handle_list_agents(sock);
        } else if (current_path_ == "/api/v1/agents" && current_method_ == "POST") {
            handle_create_agent(sock, body);
        } else {
            // Extract agent ID from path for agent-specific operations
            std::regex agent_pattern("^/api/v1/agents/([^/]+)(?:/(start|stop|status))?$");
            std::smatch matches;
            
            if (std::regex_match(current_path_, matches, agent_pattern)) {
                std::string agent_id = matches[1].str();
                std::string action = matches.size() > 2 ? matches[2].str() : "";
                
                if (action.empty() && current_method_ == "GET") {
                    handle_get_agent(sock, agent_id);
                } else if (action.empty() && current_method_ == "PUT") {
                    handle_update_agent(sock, agent_id, body);
                } else if (action.empty() && current_method_ == "DELETE") {
                    handle_delete_agent(sock, agent_id);
                } else if (action == "start" && current_method_ == "POST") {
                    handle_start_agent(sock, agent_id);
                } else if (action == "stop" && current_method_ == "POST") {
                    handle_stop_agent(sock, agent_id);
                } else if (action == "status" && current_method_ == "GET") {
                    handle_agent_status(sock, agent_id);
                } else {
                    send_error_response(sock, 404, "Endpoint not found");
                }
            } else {
                send_error_response(sock, 404, "Endpoint not found");
            }
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in AgentsRoute::handle: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_list_agents(SocketType sock) {
    json response = {
        {"status", "success"},
        {"agents", json::array()},
        {"total", 0}
    };
    
    ServerLogger::logInfo("Listing agents");
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_create_agent(SocketType sock, const std::string& body) {
    try {
        json request = json::parse(body);
        
        if (!validate_agent_config(request)) {
            send_error_response(sock, 400, "Invalid agent configuration");
            return;
        }
        
        std::string agent_id = "agent_" + std::to_string(std::time(nullptr));
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"name", request.value("name", "Unnamed Agent")},
            {"type", request.value("type", "generic")},
            {"created", true}
        };
        
        ServerLogger::logInfo("Created agent: %s", agent_id.c_str());
        send_json_response(sock, 201, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_get_agent(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"name", "Sample Agent"},
        {"type", "generic"},
        {"state", "stopped"},
        {"created_at", "2024-01-01T00:00:00Z"}
    };
    
    ServerLogger::logInfo("Getting agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_update_agent(SocketType sock, const std::string& agent_id, const std::string& body) {
    try {
        json request = json::parse(body);
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"message", "Agent updated successfully"}
        };
        
        ServerLogger::logInfo("Updated agent: %s", agent_id.c_str());
        send_json_response(sock, 200, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_delete_agent(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"message", "Agent deleted successfully"}
    };
    
    ServerLogger::logInfo("Deleted agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_start_agent(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"state", "running"},
        {"message", "Agent started successfully"}
    };
    
    ServerLogger::logInfo("Started agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_stop_agent(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"state", "stopped"},
        {"message", "Agent stopped successfully"}
    };
    
    ServerLogger::logInfo("Stopped agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_agent_status(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"state", "stopped"},
        {"uptime", 0},
        {"last_activity", "2024-01-01T00:00:00Z"},
        {"metrics", {
            {"tasks_completed", 0},
            {"tasks_failed", 0},
            {"memory_usage", "0 MB"}
        }}
    };
    
    ServerLogger::logInfo("Getting status for agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

// Helper methods
void AgentsRoute::send_response(SocketType sock, int status_code, const std::string& content) {
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    
    ::send_response(sock, status_code, content, headers);
}

void AgentsRoute::send_json_response(SocketType sock, int status_code, const nlohmann::json& data) {
    send_response(sock, status_code, data.dump());
}

void AgentsRoute::send_error_response(SocketType sock, int status_code, const std::string& error) {
    json response = {
        {"error", {
            {"message", error},
            {"type", "agent_error"},
            {"code", status_code}
        }}
    };
    send_json_response(sock, status_code, response);
}

void AgentsRoute::send_success_response(SocketType sock, const nlohmann::json& data) {
    send_json_response(sock, 200, data);
}

std::string AgentsRoute::extractIdFromPath(const std::string& path, const std::string& base_pattern) {
    std::regex pattern(base_pattern + "/([^/]+)");
    std::smatch matches;
    
    if (std::regex_search(path, matches, pattern)) {
        return matches[1].str();
    }
    
    return "";
}

bool AgentsRoute::validate_agent_config(const nlohmann::json& config) {
    // Basic validation - check for required fields
    if (!config.contains("name") || !config["name"].is_string()) {
        return false;
    }
    
    if (config.contains("type") && !config["type"].is_string()) {
        return false;
    }
    
    return true;
}

} // namespace kolosal::routes::agents

#include "kolosal/routes/agents/sequential_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <regex>
#include <iostream>

using json = nlohmann::json;

namespace kolosal::routes::agents {

SequentialRoute::SequentialRoute() {
    ServerLogger::logInfo("SequentialRoute initialized");
}

bool SequentialRoute::match(const std::string& method, const std::string& path) {
    // Match sequential workflow endpoints
    if ((method == "GET" || method == "POST") && path == "/api/v1/sequential/workflows") {
        current_method_ = method;
        current_path_ = path;
        return true;
    }
    
    // Match workflow-specific endpoints with regex
    std::regex workflow_pattern("^/api/v1/sequential/workflows/([^/]+)(?:/(execute|status))?$");
    std::smatch matches;
    
    if (std::regex_match(path, matches, workflow_pattern)) {
        if ((method == "GET" || method == "POST" || method == "DELETE")) {
            current_method_ = method;
            current_path_ = path;
            return true;
        }
    }
    
    // Handle OPTIONS for CORS
    if (method == "OPTIONS" && (path.find("/api/v1/sequential") == 0)) {
        current_method_ = method;
        current_path_ = path;
        return true;
    }
    
    return false;
}

void SequentialRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Handle OPTIONS requests for CORS
        if (current_method_ == "OPTIONS") {
            std::map<std::string, std::string> headers = {
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
                {"Access-Control-Max-Age", "86400"}
            };
            ::send_response(sock, 200, "", headers);
            return;
        }
        
        // Route to appropriate handler based on path and method
        if (current_path_ == "/api/v1/sequential/workflows" && current_method_ == "GET") {
            handle_list_workflows(sock);
        } else if (current_path_ == "/api/v1/sequential/workflows" && current_method_ == "POST") {
            handle_create_workflow(sock, body);
        } else {
            // Extract workflow ID from path for workflow-specific operations
            std::regex workflow_pattern("^/api/v1/sequential/workflows/([^/]+)(?:/(execute|status))?$");
            std::smatch matches;
            
            if (std::regex_match(current_path_, matches, workflow_pattern)) {
                std::string workflow_id = matches[1].str();
                std::string action = matches.size() > 2 ? matches[2].str() : "";
                
                if (action.empty() && current_method_ == "GET") {
                    handle_get_workflow(sock, workflow_id);
                } else if (action.empty() && current_method_ == "DELETE") {
                    handle_delete_workflow(sock, workflow_id);
                } else if (action == "execute" && current_method_ == "POST") {
                    handle_execute_workflow(sock, workflow_id, body);
                } else if (action == "status" && current_method_ == "GET") {
                    handle_get_workflow_status(sock, workflow_id);
                } else {
                    send_error_response(sock, 404, "Endpoint not found");
                }
            } else {
                send_error_response(sock, 404, "Endpoint not found");
            }
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in SequentialRoute::handle: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void SequentialRoute::handle_create_workflow(SocketType sock, const std::string& body) {
    try {
        json request = json::parse(body);
        
        if (!validate_workflow_config(request)) {
            send_error_response(sock, 400, "Invalid workflow configuration");
            return;
        }
        
        std::string workflow_id = "workflow_" + std::to_string(std::time(nullptr));
        
        json response = {
            {"status", "success"},
            {"workflow_id", workflow_id},
            {"name", request.value("name", "Unnamed Workflow")},
            {"type", "sequential"},
            {"steps", request.value("steps", json::array())},
            {"created", true}
        };
        
        ServerLogger::logInfo("Created sequential workflow: %s", workflow_id.c_str());
        send_json_response(sock, 201, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

void SequentialRoute::handle_list_workflows(SocketType sock) {
    json response = {
        {"status", "success"},
        {"workflows", json::array()},
        {"total", 0}
    };
    
    ServerLogger::logInfo("Listing sequential workflows");
    send_json_response(sock, 200, response);
}

void SequentialRoute::handle_get_workflow(SocketType sock, const std::string& workflow_id) {
    json response = {
        {"status", "success"},
        {"workflow_id", workflow_id},
        {"name", "Sample Sequential Workflow"},
        {"type", "sequential"},
        {"steps", json::array({
            {
                {"id", "step_1"},
                {"name", "First Step"},
                {"type", "agent_call"},
                {"config", {{"agent_id", "agent_1"}}}
            },
            {
                {"id", "step_2"},
                {"name", "Second Step"},
                {"type", "agent_call"},
                {"config", {{"agent_id", "agent_2"}}}
            }
        })},
        {"state", "idle"},
        {"created_at", "2024-01-01T00:00:00Z"}
    };
    
    ServerLogger::logInfo("Getting sequential workflow: %s", workflow_id.c_str());
    send_json_response(sock, 200, response);
}

void SequentialRoute::handle_execute_workflow(SocketType sock, const std::string& workflow_id, const std::string& body) {
    try {
        json request = json::parse(body);
        
        std::string execution_id = "exec_" + std::to_string(std::time(nullptr));
        
        json response = {
            {"status", "success"},
            {"workflow_id", workflow_id},
            {"execution_id", execution_id},
            {"state", "running"},
            {"started_at", "2024-01-01T00:00:00Z"},
            {"message", "Sequential workflow execution started"}
        };
        
        ServerLogger::logInfo("Executing sequential workflow: %s", workflow_id.c_str());
        send_json_response(sock, 202, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

void SequentialRoute::handle_get_workflow_status(SocketType sock, const std::string& workflow_id) {
    json response = {
        {"status", "success"},
        {"workflow_id", workflow_id},
        {"state", "idle"},
        {"current_step", nullptr},
        {"completed_steps", 0},
        {"total_steps", 2},
        {"last_execution", {
            {"execution_id", "exec_1234567890"},
            {"started_at", "2024-01-01T00:00:00Z"},
            {"completed_at", "2024-01-01T00:01:00Z"},
            {"state", "completed"},
            {"results", json::array()}
        }},
        {"metrics", {
            {"total_executions", 1},
            {"successful_executions", 1},
            {"failed_executions", 0},
            {"average_duration", 60}
        }}
    };
    
    ServerLogger::logInfo("Getting status for sequential workflow: %s", workflow_id.c_str());
    send_json_response(sock, 200, response);
}

void SequentialRoute::handle_delete_workflow(SocketType sock, const std::string& workflow_id) {
    json response = {
        {"status", "success"},
        {"workflow_id", workflow_id},
        {"message", "Sequential workflow deleted successfully"}
    };
    
    ServerLogger::logInfo("Deleted sequential workflow: %s", workflow_id.c_str());
    send_json_response(sock, 200, response);
}

// Helper methods
void SequentialRoute::send_response(SocketType sock, int status_code, const std::string& content) {
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    
    ::send_response(sock, status_code, content, headers);
}

void SequentialRoute::send_json_response(SocketType sock, int status_code, const nlohmann::json& data) {
    send_response(sock, status_code, data.dump());
}

void SequentialRoute::send_error_response(SocketType sock, int status_code, const std::string& error) {
    json response = {
        {"error", {
            {"message", error},
            {"type", "sequential_workflow_error"},
            {"code", status_code}
        }}
    };
    send_json_response(sock, status_code, response);
}

void SequentialRoute::send_success_response(SocketType sock, const nlohmann::json& data) {
    send_json_response(sock, 200, data);
}

std::string SequentialRoute::extractIdFromPath(const std::string& path, const std::string& base_pattern) {
    std::regex pattern(base_pattern + "/([^/]+)");
    std::smatch matches;
    
    if (std::regex_search(path, matches, pattern)) {
        return matches[1].str();
    }
    
    return "";
}

bool SequentialRoute::validate_workflow_config(const nlohmann::json& config) {
    // Basic validation - check for required fields
    if (!config.contains("name") || !config["name"].is_string()) {
        return false;
    }
    
    if (!config.contains("steps") || !config["steps"].is_array()) {
        return false;
    }
    
    // Validate each step
    for (const auto& step : config["steps"]) {
        if (!step.contains("name") || !step["name"].is_string()) {
            return false;
        }
        if (!step.contains("type") || !step["type"].is_string()) {
            return false;
        }
    }
    
    return true;
}

} // namespace kolosal::routes::agents

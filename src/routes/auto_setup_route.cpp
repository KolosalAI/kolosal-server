#include "kolosal/routes/auto_setup_route.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/auto_setup_manager.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

AutoSetupRoute::AutoSetupRoute() {
    ServerLogger::logInfo("Auto-setup route initialized");
}

bool AutoSetupRoute::match(const std::string& method, const std::string& path) {
    // Store the method and path for use in handle()
    last_matched_method = method;
    last_matched_path = path;
    
    // Match auto-setup related endpoints
    std::vector<std::string> patterns = {
        R"(^/api/v1/auto-setup/?$)",                    // GET status, POST trigger setup
        R"(^/api/v1/auto-setup/status/?$)",            // GET detailed status
        R"(^/api/v1/auto-setup/agent-mappings/?$)",    // GET agent name to UUID mappings
        R"(^/api/v1/auto-setup/engine-status/?$)",     // GET engine status
        R"(^/api/v1/auto-setup/validate-workflow/?$)"  // POST validate workflow with auto-mapping
    };
    
    for (const auto& pattern : patterns) {
        if (std::regex_match(path, std::regex(pattern))) {
            return true;
        }
    }
    
    return false;
}

void AutoSetupRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Store the current path for routing decisions
        current_path = last_matched_path;
        current_method = last_matched_method;
        
        // Route to appropriate handler based on method and path
        if (current_method == "GET" && (current_path == "/api/v1/auto-setup" || current_path == "/api/v1/auto-setup/")) {
            handle_get_status(sock);
        } else if (current_method == "POST" && (current_path == "/api/v1/auto-setup" || current_path == "/api/v1/auto-setup/")) {
            handle_trigger_auto_setup(sock);
        } else if (current_method == "GET" && (current_path == "/api/v1/auto-setup/status" || current_path == "/api/v1/auto-setup/status/")) {
            handle_get_status(sock);
        } else if (current_method == "GET" && (current_path == "/api/v1/auto-setup/agent-mappings" || current_path == "/api/v1/auto-setup/agent-mappings/")) {
            handle_get_agent_mappings(sock);
        } else if (current_method == "GET" && (current_path == "/api/v1/auto-setup/engine-status" || current_path == "/api/v1/auto-setup/engine-status/")) {
            handle_get_engine_status(sock);
        } else if (current_method == "POST" && (current_path == "/api/v1/auto-setup/validate-workflow" || current_path == "/api/v1/auto-setup/validate-workflow/")) {
            handle_validate_workflow(sock, body);
        } else {
            send_response(sock, 404, format_error_response("Endpoint not found", 404));
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in auto-setup route: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void AutoSetupRoute::handle_get_status(SocketType sock) {
    try {
        auto& auto_setup_manager = get_auto_setup_manager();
        
        json status_data = json::parse(auto_setup_manager.get_setup_status_json());
        
        // Add additional helpful information
        status_data["endpoints"] = {
            {"status", "/api/v1/auto-setup/status"},
            {"agent_mappings", "/api/v1/auto-setup/agent-mappings"},
            {"engine_status", "/api/v1/auto-setup/engine-status"},
            {"validate_workflow", "/api/v1/auto-setup/validate-workflow"},
            {"trigger_setup", "POST /api/v1/auto-setup"}
        };
        
        status_data["ready_for_workflows"] = 
            auto_setup_manager.is_default_engine_ready() && 
            auto_setup_manager.are_agents_available();
        
        send_response(sock, 200, format_success_response(status_data));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting auto-setup status: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void AutoSetupRoute::handle_trigger_auto_setup(SocketType sock) {
    try {
        auto& auto_setup_manager = get_auto_setup_manager();
        
        ServerLogger::logInfo("Manual auto-setup triggered via API");
        bool success = auto_setup_manager.perform_auto_setup();
        
        json response_data;
        response_data["setup_triggered"] = true;
        response_data["setup_successful"] = success;
        response_data["message"] = success ? 
            "Auto-setup completed successfully" : 
            "Auto-setup completed with some issues - check logs for details";
        
        // Include updated status
        json status_data = json::parse(auto_setup_manager.get_setup_status_json());
        response_data["status"] = status_data;
        
        int status_code = success ? 200 : 207; // 207 = Multi-Status (partial success)
        send_response(sock, status_code, format_success_response(response_data));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error triggering auto-setup: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void AutoSetupRoute::handle_get_agent_mappings(SocketType sock) {
    try {
        auto& auto_setup_manager = get_auto_setup_manager();
        
        auto mappings = auto_setup_manager.get_agent_name_to_uuid_mapping();
        
        json response_data;
        response_data["agent_mappings"] = mappings;
        response_data["agent_count"] = static_cast<int>(mappings.size());
        response_data["available_agent_names"] = auto_setup_manager.get_available_agent_names();
        
        send_response(sock, 200, format_success_response(response_data));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting agent mappings: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void AutoSetupRoute::handle_get_engine_status(SocketType sock) {
    try {
        auto& auto_setup_manager = get_auto_setup_manager();
        auto& server_api = ServerAPI::instance();
        auto& node_manager = server_api.getNodeManager();
        
        json response_data;
        response_data["default_engine_ready"] = auto_setup_manager.is_default_engine_ready();
        response_data["available_engines"] = node_manager.listEngineIds();
        response_data["engine_count"] = static_cast<int>(node_manager.listEngineIds().size());
        
        // Test default engine accessibility
        try {
            auto default_engine = node_manager.getEngine("default");
            response_data["default_engine_accessible"] = (default_engine != nullptr);
        } catch (...) {
            response_data["default_engine_accessible"] = false;
        }
        
        send_response(sock, 200, format_success_response(response_data));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting engine status: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void AutoSetupRoute::handle_validate_workflow(SocketType sock, const std::string& body) {
    try {
        if (body.empty()) {
            send_response(sock, 400, format_error_response("Request body is empty", 400));
            return;
        }
        
        json workflow_json = json::parse(body);
        auto& auto_setup_manager = get_auto_setup_manager();
        
        // Get agent mappings
        auto agent_mapping = auto_setup_manager.get_agent_name_to_uuid_mapping();
        
        json response_data;
        response_data["valid"] = true;
        response_data["issues"] = json::array();
        response_data["suggestions"] = json::array();
        response_data["mapped_workflow"] = workflow_json; // Will be modified
        
        // Check workflow structure
        if (!workflow_json.contains("steps") || !workflow_json["steps"].is_array()) {
            response_data["valid"] = false;
            response_data["issues"].push_back({
                {"type", "missing_steps"},
                {"message", "Workflow must contain a 'steps' array"}
            });
        } else {
            // Validate and map agent references
            bool any_issues = false;
            
            for (auto& step : response_data["mapped_workflow"]["steps"]) {
                if (step.contains("agent_id") && step["agent_id"].is_string()) {
                    std::string agent_id = step["agent_id"];
                    
                    // Check if this looks like a name rather than UUID
                    if (agent_id.length() != 36 || agent_id.find('-') == std::string::npos) {
                        // Try to map to UUID
                        auto it = agent_mapping.find(agent_id);
                        if (it != agent_mapping.end()) {
                            step["agent_id"] = it->second;
                            response_data["suggestions"].push_back({
                                {"type", "agent_mapped"},
                                {"message", "Agent '" + agent_id + "' automatically mapped to UUID"},
                                {"original_name", agent_id},
                                {"mapped_uuid", it->second}
                            });
                        } else {
                            any_issues = true;
                            response_data["issues"].push_back({
                                {"type", "unknown_agent"},
                                {"message", "Agent '" + agent_id + "' not found"},
                                {"agent_name", agent_id},
                                {"step_id", step.value("step_id", "unknown")}
                            });
                        }
                    }
                    // If it's already a UUID format, leave it as-is
                } else {
                    any_issues = true;
                    response_data["issues"].push_back({
                        {"type", "missing_agent_id"},
                        {"message", "Step missing 'agent_id' field"},
                        {"step_id", step.value("step_id", "unknown")}
                    });
                }
            }
            
            if (any_issues) {
                response_data["valid"] = false;
            }
        }
        
        // Add available agents for reference
        response_data["available_agents"] = auto_setup_manager.get_available_agent_names();
        
        int status_code = response_data["valid"].get<bool>() ? 200 : 400;
        send_response(sock, status_code, format_success_response(response_data));
        
    } catch (const json::parse_error& e) {
        ServerLogger::logError("JSON parse error in workflow validation: %s", e.what());
        send_response(sock, 400, format_error_response("Invalid JSON format", 400));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error validating workflow: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

std::string AutoSetupRoute::format_success_response(const nlohmann::json& data) {
    json response;
    response["success"] = true;
    response["data"] = data;
    return response.dump();
}

std::string AutoSetupRoute::format_error_response(const std::string& message, int error_code) {
    json response;
    response["success"] = false;
    response["error"] = {
        {"message", message},
        {"code", error_code}
    };
    return response.dump();
}

AutoSetupManager& AutoSetupRoute::get_auto_setup_manager() {
    auto& server_api = ServerAPI::instance();
    return server_api.getAutoSetupManager();
}

} // namespace kolosal::routes

#include "kolosal/routes/agents_route.hpp"
#include "kolosal/agents/agent_core.hpp"
#include "kolosal/agents/agent_data.hpp"
#include "kolosal/agents/yaml_config.hpp"
#include "kolosal/logger.hpp"
#include <sstream>
#include <regex>

namespace kolosal::routes {

AgentsRoute::AgentsRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    : agent_manager(manager)
    , document_service(std::make_unique<agents::DocumentAgentService>())
    , workflow_service(std::make_unique<agents::WorkflowAgentService>()) {
}

bool AgentsRoute::match(const std::string& method, const std::string& path) {
    current_method = method;
    current_path = path;
    
    // Match all agent-related paths
    std::regex agent_pattern("^/api/v1/agents");
    return std::regex_search(path, agent_pattern);
}

void AgentsRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Basic routing logic based on path and method
        if (current_path == "/api/v1/agents" && current_method == "GET") {
            handle_list_agents(sock);
        } else if (current_path == "/api/v1/agents" && current_method == "POST") {
            handle_create_agent(sock, body);
        } else if (current_path == "/api/v1/agents/system/status" && current_method == "GET") {
            handle_agent_system_status(sock);
        } else if (current_path == "/api/v1/agents/system/metrics" && current_method == "GET") {
            handle_agent_system_metrics(sock);
        } else {
            // Try to extract agent ID from path
            std::regex agent_id_pattern("^/api/v1/agents/([^/]+)(?:/(.*))?$");
            std::smatch matches;
            
            if (std::regex_match(current_path, matches, agent_id_pattern)) {
                std::string agent_id = matches[1].str();
                std::string sub_path = matches.size() > 2 ? matches[2].str() : "";
                
                if (sub_path.empty() && current_method == "GET") {
                    handle_get_agent(sock, agent_id);
                } else if (sub_path.empty() && current_method == "PUT") {
                    handle_update_agent(sock, agent_id, body);
                } else if (sub_path.empty() && current_method == "DELETE") {
                    handle_delete_agent(sock, agent_id);
                } else if (sub_path == "start" && current_method == "POST") {
                    handle_start_agent(sock, agent_id);
                } else if (sub_path == "stop" && current_method == "POST") {
                    handle_stop_agent(sock, agent_id);
                } else if (sub_path == "restart" && current_method == "POST") {
                    handle_restart_agent(sock, agent_id);
                } else if (sub_path == "status" && current_method == "GET") {
                    handle_agent_status(sock, agent_id);
                } else if (sub_path == "message" && current_method == "POST") {
                    handle_send_message_to_agent(sock, agent_id, body);
                } else {
                    send_error_response(sock, 404, "Endpoint not found");
                }
            } else {
                send_error_response(sock, 404, "Endpoint not found");
            }
        }
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Internal server error: " + std::string(e.what()));
    }
}

void AgentsRoute::setup_routes(Server& server) {
    // This method can be used to register routes with the server if needed
}

// Core agent management API endpoints
void AgentsRoute::handle_create_agent(SocketType sock, const std::string& body) {
    try {
        nlohmann::json request = nlohmann::json::parse(body);
        if (!validate_agent_config(request)) {
            send_error_response(sock, 400, "Invalid agent configuration");
            return;
        }
        
        // Convert JSON to AgentConfig
        agents::AgentConfig config;
        config.name = request.value("name", "");
        config.type = request.value("type", "generic");
        config.role = request.value("role", "");
        config.system_prompt = request.value("system_prompt", "");
        config.auto_start = request.value("auto_start", true);
        config.max_concurrent_jobs = request.value("max_concurrent_jobs", 5);
        config.heartbeat_interval_seconds = request.value("heartbeat_interval_seconds", 5);
        
        if (request.contains("capabilities") && request["capabilities"].is_array()) {
            for (const auto& cap : request["capabilities"]) {
                config.capabilities.push_back(cap.get<std::string>());
            }
        }
        
        if (request.contains("functions") && request["functions"].is_array()) {
            for (const auto& func : request["functions"]) {
                config.functions.push_back(func.get<std::string>());
            }
        }
        
        // Create agent through agent_manager
        std::string agent_id = agent_manager->create_agent_from_config(config);
        
        if (agent_id.empty()) {
            send_error_response(sock, 500, "Failed to create agent");
            return;
        }
        
        // Start agent if auto_start is true
        if (config.auto_start) {
            agent_manager->start_agent(agent_id);
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["name"] = config.name;
        response["type"] = config.type;
        response["running"] = config.auto_start;
        send_json_response(sock, 201, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_list_agents(SocketType sock) {
    try {
        auto agent_ids = agent_manager->list_agents();
        nlohmann::json agents_array = nlohmann::json::array();
        
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent) {
                nlohmann::json agent_info = agent_to_json(agent);
                agent_info["id"] = agent_id;
                agents_array.push_back(agent_info);
            }
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agents"] = agents_array;
        response["total_count"] = agent_ids.size();
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to list agents: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_get_agent(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent"] = agent_to_json(agent);
        response["agent"]["id"] = agent_id;
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to retrieve agent: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_update_agent(SocketType sock, const std::string& agent_id, const std::string& body) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        nlohmann::json request = nlohmann::json::parse(body);
        
        // For now, we'll only support updating capabilities
        // More sophisticated updates would require rebuilding the agent
        if (request.contains("capabilities") && request["capabilities"].is_array()) {
            // Note: AgentCore capabilities are read-only after creation
            // This would require a more sophisticated update mechanism
            send_error_response(sock, 501, "Agent capability updates not yet supported - recreate agent instead");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["message"] = "Agent configuration is read-only after creation. Delete and recreate for changes.";
        send_json_response(sock, 200, response);
    } catch (const nlohmann::json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to update agent: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_delete_agent(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        bool success = agent_manager->delete_agent(agent_id);
        if (!success) {
            send_error_response(sock, 500, "Failed to delete agent");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["message"] = "Agent deleted successfully";
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to delete agent: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_agent_system_status(SocketType sock) {
    try {
        std::string status = agent_manager->get_system_status();
        auto agent_ids = agent_manager->list_agents();
        
        int running_count = 0;
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent && agent->is_running()) {
                running_count++;
            }
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["system_running"] = agent_manager->is_running();
        response["total_agents"] = agent_ids.size();
        response["running_agents"] = running_count;
        response["stopped_agents"] = agent_ids.size() - running_count;
        response["detailed_status"] = status;
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get system status: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_agent_system_metrics(SocketType sock) {
    try {
        auto agent_ids = agent_manager->list_agents();
        nlohmann::json metrics;
        nlohmann::json agent_metrics = nlohmann::json::array();
        
        int total_agents = 0;
        int running_agents = 0;
        int stopped_agents = 0;
        
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent) {
                total_agents++;
                nlohmann::json agent_metric = create_agent_metrics(agent);
                agent_metric["id"] = agent_id;
                agent_metric["name"] = agent->get_agent_name();
                agent_metric["type"] = agent->get_agent_type();
                agent_metric["running"] = agent->is_running();
                agent_metric["capabilities_count"] = agent->get_capabilities().size();
                agent_metric["functions_count"] = agent->get_function_manager()->get_function_names().size();
                
                if (agent->is_running()) {
                    running_agents++;
                } else {
                    stopped_agents++;
                }
                
                agent_metrics.push_back(agent_metric);
            }
        }
        
        metrics["total_agents"] = total_agents;
        metrics["running_agents"] = running_agents;
        metrics["stopped_agents"] = stopped_agents;
        metrics["system_running"] = agent_manager->is_running();
        metrics["agents"] = agent_metrics;
        
        nlohmann::json response;
        response["status"] = "success";
        response["metrics"] = metrics;
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get system metrics: " + std::string(e.what()));
    }
}

// Agent lifecycle management
void AgentsRoute::handle_start_agent(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        if (agent->is_running()) {
            nlohmann::json response;
            response["status"] = "success";
            response["agent_id"] = agent_id;
            response["message"] = "Agent is already running";
            response["running"] = true;
            send_json_response(sock, 200, response);
            return;
        }
        
        bool success = agent_manager->start_agent(agent_id);
        if (!success) {
            send_error_response(sock, 500, "Failed to start agent");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["message"] = "Agent started successfully";
        response["running"] = true;
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to start agent: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_stop_agent(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        if (!agent->is_running()) {
            nlohmann::json response;
            response["status"] = "success";
            response["agent_id"] = agent_id;
            response["message"] = "Agent is already stopped";
            response["running"] = false;
            send_json_response(sock, 200, response);
            return;
        }
        
        bool success = agent_manager->stop_agent(agent_id);
        if (!success) {
            send_error_response(sock, 500, "Failed to stop agent");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["message"] = "Agent stopped successfully";
        response["running"] = false;
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to stop agent: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_restart_agent(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        // Stop the agent first if it's running
        bool was_running = agent->is_running();
        if (was_running) {
            bool stop_success = agent_manager->stop_agent(agent_id);
            if (!stop_success) {
                send_error_response(sock, 500, "Failed to stop agent for restart");
                return;
            }
        }
        
        // Start the agent
        bool start_success = agent_manager->start_agent(agent_id);
        if (!start_success) {
            send_error_response(sock, 500, "Failed to start agent after restart");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["message"] = "Agent restarted successfully";
        response["was_running"] = was_running;
        response["running"] = true;
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to restart agent: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_agent_status(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["name"] = agent->get_agent_name();
        response["type"] = agent->get_agent_type();
        response["running"] = agent->is_running();
        response["capabilities"] = agent->get_capabilities();
        response["function_count"] = agent->get_function_manager()->get_function_names().size();
        response["functions"] = agent->get_function_manager()->get_function_names();
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get agent status: " + std::string(e.what()));
    }
}

// Agent capabilities and functions (stub implementations)
void AgentsRoute::handle_get_agent_capabilities(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["capabilities"] = agent->get_capabilities();
        response["capability_count"] = agent->get_capabilities().size();
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get agent capabilities: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_list_agent_functions(SocketType sock, const std::string& agent_id) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        auto function_names = agent->get_function_manager()->get_function_names();
        nlohmann::json functions_array = nlohmann::json::array();
        
        for (const auto& func_name : function_names) {
            nlohmann::json func_info;
            func_info["name"] = func_name;
            func_info["description"] = agent->get_function_manager()->get_function_description(func_name);
            functions_array.push_back(func_info);
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["functions"] = functions_array;
        response["function_count"] = function_names.size();
        response["tools_summary"] = agent->get_function_manager()->get_available_tools_summary();
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to list agent functions: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_execute_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        if (!agent->is_running()) {
            send_error_response(sock, 400, "Agent is not running");
            return;
        }
        
        if (!agent->get_function_manager()->has_function(function_name)) {
            send_error_response(sock, 404, "Function not found on agent: " + function_name);
            return;
        }
        
        kolosal::agents::AgentData params;
        if (!body.empty()) {
            try {
                nlohmann::json request = nlohmann::json::parse(body);
                // Convert JSON parameters to AgentData
                for (auto& [key, value] : request.items()) {
                    if (value.is_string()) {
                        params.set(key, value.get<std::string>());
                    } else if (value.is_number()) {
                        params.set(key, std::to_string(value.get<double>()));
                    } else {
                        params.set(key, value.dump());
                    }
                }
            } catch (const nlohmann::json::parse_error& e) {
                send_error_response(sock, 400, "Invalid JSON parameters: " + std::string(e.what()));
                return;
            }
        }
        
        auto result = agent->execute_function(function_name, params);
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["function_name"] = function_name;
        response["execution_success"] = result.success;
        response["execution_time_ms"] = result.execution_time_ms;
        
        if (!result.success) {
            response["error_message"] = result.error_message;
        }
        
        if (!result.llm_response.empty()) {
            response["llm_response"] = result.llm_response;
        }
        
        // Convert result data back to JSON (placeholder for now)
        nlohmann::json result_data;
        response["result_data"] = result_data;
        
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to execute agent function: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_test_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

// Agent messaging with model selection
void AgentsRoute::handle_send_message_to_agent(SocketType sock, const std::string& agent_id, const std::string& body) {
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        if (!agent->is_running()) {
            send_error_response(sock, 400, "Agent is not running");
            return;
        }
        
        nlohmann::json request = nlohmann::json::parse(body);
        if (!validate_message_payload(request)) {
            send_error_response(sock, 400, "Invalid message payload");
            return;
        }
        
        std::string message = request.value("message", "");
        std::string function_name = request.value("function", "");
        
        if (function_name.empty()) {
            // If no specific function, try to send as a general message
            // This would require implementing a general message handler
            send_error_response(sock, 400, "Function name required for agent communication");
            return;
        }
        
        // Execute function on the agent
        kolosal::agents::AgentData params;
        if (request.contains("parameters")) {
            // Convert JSON parameters to AgentData
            for (auto& [key, value] : request["parameters"].items()) {
                if (value.is_string()) {
                    params.set(key, value.get<std::string>());
                } else if (value.is_number()) {
                    params.set(key, std::to_string(value.get<double>()));
                } else {
                    params.set(key, value.dump());
                }
            }
        }
        
        if (!message.empty()) {
            params.set("message", message);
        }
        
        auto result = agent->execute_function(function_name, params);
        
        nlohmann::json response;
        response["status"] = "success";
        response["agent_id"] = agent_id;
        response["function_executed"] = function_name;
        response["execution_success"] = result.success;
        response["execution_time_ms"] = result.execution_time_ms;
        
        if (!result.success) {
            response["error"] = result.error_message;
        }
        
        if (!result.llm_response.empty()) {
            response["llm_response"] = result.llm_response;
        }
        
        // Convert result data back to JSON
        nlohmann::json result_data;
        // Note: AgentData doesn't have a direct JSON conversion method
        // This would need to be implemented based on AgentData structure
        response["result_data"] = result_data; // Placeholder
        
        send_json_response(sock, 200, response);
    } catch (const nlohmann::json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to send message to agent: " + std::string(e.what()));
    }
}

// Agent templates and presets (stub implementations)
void AgentsRoute::handle_list_agent_templates(SocketType sock) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

void AgentsRoute::handle_create_agent_from_template(SocketType sock, const std::string& template_name, const std::string& body) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

// Bulk operations (stub implementations)
void AgentsRoute::handle_bulk_start_agents(SocketType sock, const std::string& body) {
    try {
        nlohmann::json request = nlohmann::json::parse(body);
        
        if (!request.contains("agent_ids") || !request["agent_ids"].is_array()) {
            send_error_response(sock, 400, "Missing or invalid agent_ids array");
            return;
        }
        
        nlohmann::json results = nlohmann::json::array();
        int success_count = 0;
        int error_count = 0;
        
        for (const auto& agent_id_json : request["agent_ids"]) {
            if (!agent_id_json.is_string()) {
                continue;
            }
            
            std::string agent_id = agent_id_json.get<std::string>();
            nlohmann::json result;
            result["agent_id"] = agent_id;
            
            auto agent = agent_manager->get_agent(agent_id);
            if (!agent) {
                result["success"] = false;
                result["error"] = "Agent not found";
                error_count++;
            } else if (agent->is_running()) {
                result["success"] = true;
                result["message"] = "Agent already running";
                success_count++;
            } else {
                bool started = agent_manager->start_agent(agent_id);
                result["success"] = started;
                if (started) {
                    result["message"] = "Started successfully";
                    success_count++;
                } else {
                    result["error"] = "Failed to start";
                    error_count++;
                }
            }
            
            results.push_back(result);
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["results"] = results;
        response["success_count"] = success_count;
        response["error_count"] = error_count;
        response["total_processed"] = success_count + error_count;
        send_json_response(sock, 200, response);
    } catch (const nlohmann::json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to bulk start agents: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_bulk_stop_agents(SocketType sock, const std::string& body) {
    try {
        nlohmann::json request = nlohmann::json::parse(body);
        
        if (!request.contains("agent_ids") || !request["agent_ids"].is_array()) {
            send_error_response(sock, 400, "Missing or invalid agent_ids array");
            return;
        }
        
        nlohmann::json results = nlohmann::json::array();
        int success_count = 0;
        int error_count = 0;
        
        for (const auto& agent_id_json : request["agent_ids"]) {
            if (!agent_id_json.is_string()) {
                continue;
            }
            
            std::string agent_id = agent_id_json.get<std::string>();
            nlohmann::json result;
            result["agent_id"] = agent_id;
            
            auto agent = agent_manager->get_agent(agent_id);
            if (!agent) {
                result["success"] = false;
                result["error"] = "Agent not found";
                error_count++;
            } else if (!agent->is_running()) {
                result["success"] = true;
                result["message"] = "Agent already stopped";
                success_count++;
            } else {
                bool stopped = agent_manager->stop_agent(agent_id);
                result["success"] = stopped;
                if (stopped) {
                    result["message"] = "Stopped successfully";
                    success_count++;
                } else {
                    result["error"] = "Failed to stop";
                    error_count++;
                }
            }
            
            results.push_back(result);
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["results"] = results;
        response["success_count"] = success_count;
        response["error_count"] = error_count;
        response["total_processed"] = success_count + error_count;
        send_json_response(sock, 200, response);
    } catch (const nlohmann::json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to bulk stop agents: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_bulk_delete_agents(SocketType sock, const std::string& body) {
    try {
        nlohmann::json request = nlohmann::json::parse(body);
        
        if (!request.contains("agent_ids") || !request["agent_ids"].is_array()) {
            send_error_response(sock, 400, "Missing or invalid agent_ids array");
            return;
        }
        
        nlohmann::json results = nlohmann::json::array();
        int success_count = 0;
        int error_count = 0;
        
        for (const auto& agent_id_json : request["agent_ids"]) {
            if (!agent_id_json.is_string()) {
                continue;
            }
            
            std::string agent_id = agent_id_json.get<std::string>();
            nlohmann::json result;
            result["agent_id"] = agent_id;
            
            auto agent = agent_manager->get_agent(agent_id);
            if (!agent) {
                result["success"] = false;
                result["error"] = "Agent not found";
                error_count++;
            } else {
                bool deleted = agent_manager->delete_agent(agent_id);
                result["success"] = deleted;
                if (deleted) {
                    result["message"] = "Deleted successfully";
                    success_count++;
                } else {
                    result["error"] = "Failed to delete";
                    error_count++;
                }
            }
            
            results.push_back(result);
        }
        
        nlohmann::json response;
        response["status"] = "success";
        response["results"] = results;
        response["success_count"] = success_count;
        response["error_count"] = error_count;
        response["total_processed"] = success_count + error_count;
        send_json_response(sock, 200, response);
    } catch (const nlohmann::json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to bulk delete agents: " + std::string(e.what()));
    }
}

// Document agent service endpoints (delegate to service)
void AgentsRoute::handle_bulk_documents(SocketType sock, const std::string& body) {
    if (document_service) {
        // Implementation would delegate to document_service
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

void AgentsRoute::handle_bulk_retrieval(SocketType sock, const std::string& body) {
    if (document_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

void AgentsRoute::handle_document_search(SocketType sock, const std::string& body) {
    if (document_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

void AgentsRoute::handle_document_upload(SocketType sock, const std::string& body) {
    if (document_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

void AgentsRoute::handle_list_collections(SocketType sock) {
    if (document_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

void AgentsRoute::handle_create_collection(SocketType sock, const std::string& body) {
    if (document_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

void AgentsRoute::handle_delete_collection(SocketType sock, const std::string& collection_name) {
    if (document_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

void AgentsRoute::handle_get_collection_info(SocketType sock, const std::string& collection_name) {
    if (document_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Document service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Document service not available");
    }
}

// Workflow agent service endpoints (delegate to service)
void AgentsRoute::handle_create_workflow(SocketType sock, const std::string& body) {
    if (workflow_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Workflow service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Workflow service not available");
    }
}

void AgentsRoute::handle_execute_workflow(SocketType sock, const std::string& body) {
    if (workflow_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Workflow service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Workflow service not available");
    }
}

void AgentsRoute::handle_get_workflow_status(SocketType sock, const std::string& workflow_id) {
    if (workflow_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Workflow service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Workflow service not available");
    }
}

void AgentsRoute::handle_list_workflows(SocketType sock) {
    if (workflow_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Workflow service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Workflow service not available");
    }
}

void AgentsRoute::handle_delete_workflow(SocketType sock, const std::string& workflow_id) {
    if (workflow_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Workflow service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Workflow service not available");
    }
}

void AgentsRoute::handle_rag_workflow(SocketType sock, const std::string& body) {
    if (workflow_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Workflow service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Workflow service not available");
    }
}

void AgentsRoute::handle_rag_search(SocketType sock, const std::string& body) {
    if (workflow_service) {
        nlohmann::json response;
        response["status"] = "success";
        response["message"] = "Workflow service delegation not yet implemented";
        send_json_response(sock, 501, response);
    } else {
        send_error_response(sock, 500, "Workflow service not available");
    }
}

// Session management endpoints (stub implementations)
void AgentsRoute::handle_create_session(SocketType sock, const std::string& body) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

void AgentsRoute::handle_get_session(SocketType sock, const std::string& session_id) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

void AgentsRoute::handle_list_sessions(SocketType sock) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

void AgentsRoute::handle_delete_session(SocketType sock, const std::string& session_id) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

void AgentsRoute::handle_session_history(SocketType sock, const std::string& session_id) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

// Orchestration endpoints (stub implementations)
void AgentsRoute::handle_create_orchestration(SocketType sock, const std::string& body) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

void AgentsRoute::handle_execute_orchestration(SocketType sock, const std::string& plan_id, const std::string& body) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

void AgentsRoute::handle_orchestration_status(SocketType sock, const std::string& plan_id) {
    nlohmann::json response;
    response["status"] = "success";
    response["message"] = "Not yet implemented";
    send_json_response(sock, 501, response);
}

// Helper methods
std::string AgentsRoute::format_error_response(const std::string& error, int code) {
    nlohmann::json response;
    response["status"] = "error";
    response["error"] = error;
    response["code"] = code;
    return response.dump();
}

std::string AgentsRoute::format_success_response(const nlohmann::json& data) {
    nlohmann::json response;
    response["status"] = "success";
    if (!data.empty()) {
        response["data"] = data;
    }
    return response.dump();
}

bool AgentsRoute::validate_agent_config(const nlohmann::json& config) {
    // Check for required fields
    if (!config.contains("name") || !config["name"].is_string() || config["name"].get<std::string>().empty()) {
        return false;
    }
    
    // Type is optional, defaults to "generic"
    if (config.contains("type") && (!config["type"].is_string() || config["type"].get<std::string>().empty())) {
        return false;
    }
    
    // Validate capabilities array if present
    if (config.contains("capabilities")) {
        if (!config["capabilities"].is_array()) {
            return false;
        }
        for (const auto& cap : config["capabilities"]) {
            if (!cap.is_string()) {
                return false;
            }
        }
    }
    
    // Validate functions array if present
    if (config.contains("functions")) {
        if (!config["functions"].is_array()) {
            return false;
        }
        for (const auto& func : config["functions"]) {
            if (!func.is_string()) {
                return false;
            }
        }
    }
    
    // Validate boolean fields if present
    if (config.contains("auto_start") && !config["auto_start"].is_boolean()) {
        return false;
    }
    
    // Validate numeric fields if present
    if (config.contains("max_concurrent_jobs") && !config["max_concurrent_jobs"].is_number_integer()) {
        return false;
    }
    
    if (config.contains("heartbeat_interval_seconds") && !config["heartbeat_interval_seconds"].is_number_integer()) {
        return false;
    }
    
    return true;
}

bool AgentsRoute::validate_message_payload(const nlohmann::json& payload) {
    // Check for either message or function field
    bool has_message = payload.contains("message") && payload["message"].is_string();
    bool has_function = payload.contains("function") && payload["function"].is_string();
    
    if (!has_message && !has_function) {
        return false;
    }
    
    // Validate parameters if present
    if (payload.contains("parameters")) {
        if (!payload["parameters"].is_object()) {
            return false;
        }
    }
    
    return true;
}

nlohmann::json AgentsRoute::agent_to_json(const std::shared_ptr<agents::AgentCore>& agent) {
    if (!agent) {
        return nlohmann::json::object();
    }
    
    nlohmann::json result;
    result["name"] = agent->get_agent_name();
    result["type"] = agent->get_agent_type();
    result["running"] = agent->is_running();
    result["capabilities"] = agent->get_capabilities();
    result["capability_count"] = agent->get_capabilities().size();
    
    auto function_names = agent->get_function_manager()->get_function_names();
    result["functions"] = function_names;
    result["function_count"] = function_names.size();
    
    return result;
}

nlohmann::json AgentsRoute::create_agent_metrics(const std::shared_ptr<agents::AgentCore>& agent) {
    if (!agent) {
        return nlohmann::json::object();
    }
    
    nlohmann::json result;
    result["name"] = agent->get_agent_name();
    result["type"] = agent->get_agent_type();
    result["running"] = agent->is_running();
    result["capabilities_count"] = agent->get_capabilities().size();
    result["functions_count"] = agent->get_function_manager()->get_function_names().size();
    
    // Additional metrics that could be useful
    result["has_function_manager"] = (agent->get_function_manager() != nullptr);
    result["has_job_manager"] = (agent->get_job_manager() != nullptr);
    result["has_event_system"] = (agent->get_event_system() != nullptr);
    
    return result;
}

// Private helper methods
void AgentsRoute::send_response(SocketType sock, int status_code, const std::string& content) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " ";
    
    switch (status_code) {
        case 200: response << "OK"; break;
        case 201: response << "Created"; break;
        case 400: response << "Bad Request"; break;
        case 404: response << "Not Found"; break;
        case 500: response << "Internal Server Error"; break;
        case 501: response << "Not Implemented"; break;
        default: response << "Unknown"; break;
    }
    
    response << "\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "\r\n";
    response << content;
    
    std::string response_str = response.str();
    send(sock, response_str.c_str(), response_str.length(), 0);
}

void AgentsRoute::send_json_response(SocketType sock, int status_code, const nlohmann::json& data) {
    send_response(sock, status_code, data.dump());
}

void AgentsRoute::send_error_response(SocketType sock, int status_code, const std::string& error) {
    nlohmann::json response;
    response["status"] = "error";
    response["error"] = error;
    send_json_response(sock, status_code, response);
}

void AgentsRoute::send_success_response(SocketType sock, const nlohmann::json& data) {
    nlohmann::json response;
    response["status"] = "success";
    if (!data.empty()) {
        response["data"] = data;
    }
    send_json_response(sock, 200, response);
}

// Route parsing helpers
std::string AgentsRoute::extractIdFromPath(const std::string& path, const std::string& base_pattern) {
    std::regex pattern(base_pattern + "/([^/]+)");
    std::smatch matches;
    if (std::regex_match(path, matches, pattern)) {
        return matches[1].str();
    }
    return "";
}

bool AgentsRoute::matchesPattern(const std::string& path, const std::string& pattern) {
    std::regex regex_pattern(pattern);
    return std::regex_match(path, regex_pattern);
}

} // namespace kolosal::routes

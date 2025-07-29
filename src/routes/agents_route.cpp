#include "kolosal/routes/agents_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/agents/agent_data.hpp"
#include "kolosal/agents/yaml_config.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <sstream>
#include <regex>
#include <chrono>
#include <thread>

using json = nlohmann::json;

namespace kolosal::routes {

AgentsRoute::AgentsRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager) 
    : agent_manager(manager) {
}

void AgentsRoute::setup_routes(Server& server) {
    ServerLogger::logInfo("Setting up comprehensive agent management routes");
    
    // The route matching will be handled by the main route dispatcher
    // This is a placeholder for route registration
}

// Helper methods
void AgentsRoute::send_response(SocketType sock, int status_code, const std::string& content) {
    ::send_response(sock, status_code, content);
}

void AgentsRoute::send_json_response(SocketType sock, int status_code, const nlohmann::json& data) {
    send_response(sock, status_code, data.dump());
}

void AgentsRoute::send_error_response(SocketType sock, int status_code, const std::string& error) {
    json response = {
        {"success", false},
        {"error", error},
        {"code", status_code}
    };
    send_json_response(sock, status_code, response);
}

void AgentsRoute::send_success_response(SocketType sock, const nlohmann::json& data) {
    json response = {
        {"success", true},
        {"data", data}
    };
    send_json_response(sock, 200, response);
}

// Core agent management API implementations
void AgentsRoute::handle_create_agent(SocketType sock, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto json_data = json::parse(body);
        
        if (!validate_agent_config(json_data)) {
            send_error_response(sock, 400, "Invalid agent configuration");
            return;
        }
        
        // Convert JSON to AgentConfig
        agents::AgentConfig config;
        config.id = json_data.value("agent_id", "");
        config.name = json_data.value("name", "");
        config.type = json_data.value("type", "general");
        config.auto_start = json_data.value("auto_start", false);
        config.max_concurrent_jobs = json_data.value("max_concurrent_requests", 1);
        
        if (json_data.contains("llm_config")) {
            auto llm_config = json_data["llm_config"];
            config.llm_config.model_name = llm_config.value("model_name", "");
            config.llm_config.temperature = llm_config.value("temperature", 0.7);
            config.llm_config.max_tokens = llm_config.value("max_tokens", 1000);
            config.llm_config.instruction = llm_config.value("system_prompt", "");
        }
        
        if (json_data.contains("capabilities")) {
            config.capabilities = json_data["capabilities"];
        }
        
        if (json_data.contains("functions")) {
            config.functions = json_data["functions"];
        }
        
        // Create the agent
        std::string agent_id = agent_manager->create_agent_from_config(config);
        
        if (agent_id.empty()) {
            send_error_response(sock, 500, "Failed to create agent");
            return;
        }
        
        // Auto-start if requested
        if (config.auto_start) {
            agent_manager->start_agent(agent_id);
        }
        
        json response = {
            {"agent_id", agent_id},
            {"name", config.name},
            {"type", config.type},
            {"auto_started", config.auto_start},
            {"message", "Agent created successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_list_agents(SocketType sock) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent_ids = agent_manager->list_agents();
        json agents_array = json::array();
        
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent) {
                agents_array.push_back(agent_to_json(agent));
            }
        }
        
        json response = {
            {"agents", agents_array},
            {"count", agents_array.size()},
            {"system_running", agent_manager->is_running()}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_get_agent(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        auto agent_json = agent_to_json(agent);
        send_success_response(sock, agent_json);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_update_agent(SocketType sock, const std::string& agent_id, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        auto json_data = json::parse(body);
        
        // Update agent configuration
        // This is a simplified implementation - in practice, you'd need more sophisticated update logic
        bool success = true;
        
        if (json_data.contains("name")) {
            // Update agent name - this would require agent-specific update methods
            success = false; // Placeholder - implement actual update logic
        }
        
        if (!success) {
            send_error_response(sock, 500, "Failed to update agent configuration");
            return;
        }
        
        json response = {
            {"agent_id", agent_id},
            {"message", "Agent updated successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_delete_agent(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        bool success = agent_manager->delete_agent(agent_id);
        
        if (!success) {
            send_error_response(sock, 404, "Agent not found or could not be deleted");
            return;
        }
        
        json response = {
            {"agent_id", agent_id},
            {"message", "Agent deleted successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_agent_system_status(SocketType sock) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent_ids = agent_manager->list_agents();
        int running_agents = 0;
        int total_agents = agent_ids.size();
        
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent && agent->is_running()) {
                running_agents++;
            }
        }
        
        json status = {
            {"system_running", agent_manager->is_running()},
            {"total_agents", total_agents},
            {"running_agents", running_agents},
            {"stopped_agents", total_agents - running_agents},
            {"uptime_seconds", 0}, // TODO: Calculate actual uptime
            {"memory_usage_mb", 0}, // TODO: Calculate memory usage
            {"cpu_usage_percent", 0.0}, // TODO: Calculate CPU usage
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        send_success_response(sock, status);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_agent_system_metrics(SocketType sock) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent_ids = agent_manager->list_agents();
        int running_agents = 0;
        int total_agents = agent_ids.size();
        
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent && agent->is_running()) {
                running_agents++;
            }
        }
        
        json metrics = {
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"agent_system", {
                {"manager_status", agent_manager->is_running() ? "running" : "stopped"},
                {"total_agents", total_agents},
                {"running_agents", running_agents},
                {"stopped_agents", total_agents - running_agents},
                {"orchestrator_status", "not_available"}
            }}
        };
        
        send_success_response(sock, metrics);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Agent lifecycle management
void AgentsRoute::handle_start_agent(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        bool success = agent_manager->start_agent(agent_id);
        
        if (!success) {
            send_error_response(sock, 404, "Agent not found or could not be started");
            return;
        }
        
        json response = {
            {"agent_id", agent_id},
            {"status", "started"},
            {"message", "Agent started successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_stop_agent(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        bool success = agent_manager->stop_agent(agent_id);
        
        if (!success) {
            send_error_response(sock, 404, "Agent not found or could not be stopped");
            return;
        }
        
        json response = {
            {"agent_id", agent_id},
            {"status", "stopped"},
            {"message", "Agent stopped successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_restart_agent(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        // Stop then start the agent
        bool stop_success = agent_manager->stop_agent(agent_id);
        if (!stop_success) {
            send_error_response(sock, 404, "Agent not found or could not be stopped");
            return;
        }
        
        // Small delay to ensure clean shutdown
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool start_success = agent_manager->start_agent(agent_id);
        if (!start_success) {
            send_error_response(sock, 500, "Agent stopped but could not be restarted");
            return;
        }
        
        json response = {
            {"agent_id", agent_id},
            {"status", "restarted"},
            {"message", "Agent restarted successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_agent_status(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        json status = {
            {"agent_id", agent_id},
            {"name", agent->get_agent_name()},
            {"type", agent->get_agent_type()},
            {"running", agent->is_running()},
            {"uptime_seconds", 0}, // TODO: Calculate uptime
            {"request_count", 0}, // TODO: Get request count
            {"error_count", 0}, // TODO: Get error count
            {"last_activity", 0}, // TODO: Get last activity timestamp
            {"memory_usage_mb", 0}, // TODO: Get memory usage
            {"cpu_usage_percent", 0.0} // TODO: Get CPU usage
        };
        
        send_success_response(sock, status);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Agent capabilities and functions
void AgentsRoute::handle_get_agent_capabilities(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        auto capabilities = agent->get_capabilities();
        
        json response = {
            {"agent_id", agent_id},
            {"capabilities", capabilities}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_list_agent_functions(SocketType sock, const std::string& agent_id) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        auto functions = agent->get_function_manager()->get_function_names();
        
        json response = {
            {"agent_id", agent_id},
            {"functions", functions},
            {"function_count", functions.size()}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_execute_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        if (!agent->is_running()) {
            send_error_response(sock, 409, "Agent is not running");
            return;
        }
        
        // Parse function parameters
        agents::AgentData function_data;
        function_data.set("function", function_name);
        
        if (!body.empty()) {
            auto json_params = json::parse(body);
            
            // If the JSON has a "parameters" object, extract its contents to root level
            if (json_params.contains("parameters") && json_params["parameters"].is_object()) {
                for (auto& [key, value] : json_params["parameters"].items()) {
                    if (value.is_string()) {
                        function_data.set(key, value.get<std::string>());
                    } else if (value.is_number_integer()) {
                        function_data.set(key, value.get<int>());
                    } else if (value.is_number_float()) {
                        function_data.set(key, value.get<double>());
                    } else if (value.is_boolean()) {
                        function_data.set(key, value.get<bool>());
                    } else if (value.is_array()) {
                        // Convert array to AgentData format
                        std::vector<std::string> string_array;
                        for (const auto& item : value) {
                            if (item.is_string()) {
                                string_array.push_back(item.get<std::string>());
                            }
                        }
                        if (!string_array.empty()) {
                            function_data.set(key, string_array);
                        }
                    } else {
                        // For complex objects, store as JSON string
                        function_data.set(key, value.dump());
                    }
                }
            } else {
                // If no "parameters" object, set the entire JSON at root level
                for (auto& [key, value] : json_params.items()) {
                    if (value.is_string()) {
                        function_data.set(key, value.get<std::string>());
                    } else if (value.is_number_integer()) {
                        function_data.set(key, value.get<int>());
                    } else if (value.is_number_float()) {
                        function_data.set(key, value.get<double>());
                    } else if (value.is_boolean()) {
                        function_data.set(key, value.get<bool>());
                    } else if (value.is_array()) {
                        // Convert array to AgentData format
                        std::vector<std::string> string_array;
                        for (const auto& item : value) {
                            if (item.is_string()) {
                                string_array.push_back(item.get<std::string>());
                            }
                        }
                        if (!string_array.empty()) {
                            function_data.set(key, string_array);
                        }
                    } else {
                        // For complex objects, store as JSON string
                        function_data.set(key, value.dump());
                    }
                }
            }
        }
        
        // Execute the function
        auto result = agent->get_function_manager()->execute_function(function_name, function_data);
        
        json response;
        response["agent_id"] = agent_id;
        response["function_name"] = function_name;
        response["success"] = result.success;
        response["result"] = result.result_data.to_json();
        response["execution_time_ms"] = result.execution_time_ms;
        response["error_message"] = result.error_message;
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_test_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        // Check if function exists
        auto functions = agent->get_function_manager()->get_function_names();
        bool function_exists = std::find(functions.begin(), functions.end(), function_name) != functions.end();
        
        if (!function_exists) {
            send_error_response(sock, 404, "Function not found");
            return;
        }
        
        // Validate parameters without executing
        json test_result = {
            {"agent_id", agent_id},
            {"function_name", function_name},
            {"function_exists", true},
            {"agent_running", agent->is_running()},
            {"can_execute", agent->is_running()},
            {"test_timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        if (!body.empty()) {
            try {
                auto json_params = json::parse(body);
                test_result["parameters_valid"] = true;
                test_result["parameters"] = json_params;
            } catch (const json::parse_error& e) {
                test_result["parameters_valid"] = false;
                test_result["parameter_error"] = e.what();
            }
        } else {
            test_result["parameters_valid"] = true;
            test_result["parameters"] = json::object();
        }
        
        send_success_response(sock, test_result);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Agent messaging with model selection
void AgentsRoute::handle_send_message_to_agent(SocketType sock, const std::string& agent_id, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        if (!agent->is_running()) {
            send_error_response(sock, 409, "Agent is not running");
            return;
        }
        
        // Parse the request
        auto json_data = json::parse(body);
        
        if (!json_data.contains("message") || json_data["message"].empty()) {
            send_error_response(sock, 400, "Message content is required");
            return;
        }
        
        std::string message = json_data["message"];
        std::string model_name = json_data.value("model", "qwen3-0.6b");  // Default model
        
        // Extract optional parameters
        double temperature = json_data.value("temperature", 0.7);
        int max_tokens = json_data.value("max_tokens", 2048);
        
        // Execute inference function with the specified model
        agents::AgentData function_data;
        function_data.set("prompt", message);
        function_data.set("model_id", model_name);
        function_data.set("temperature", temperature);
        function_data.set("max_tokens", max_tokens);
        
        auto result = agent->get_function_manager()->execute_function("inference", function_data);
        
        json response;
        response["agent_id"] = agent_id;
        response["message"] = message;
        response["model_used"] = model_name;
        response["success"] = result.success;
        response["execution_time_ms"] = result.execution_time_ms;
        response["error_message"] = result.error_message;
        
        if (result.success) {
            response["response"] = result.result_data.get_string("text");
            response["tokens_generated"] = result.result_data.get_int("tokens_generated", 0);
            response["tokens_per_second"] = result.result_data.get_double("tokens_per_second", 0.0);
        }
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Agent templates
void AgentsRoute::handle_list_agent_templates(SocketType sock) {
    try {
        json templates = {
            {
                {"name", "research_agent"},
                {"description", "Agent specialized in research and information gathering"},
                {"type", "research"},
                {"capabilities", {"web_search", "document_analysis", "summarization"}},
                {"default_functions", {"search", "analyze", "summarize"}}
            },
            {
                {"name", "writer_agent"},
                {"description", "Agent specialized in content creation and writing"},
                {"type", "writer"},
                {"capabilities", {"text_generation", "editing", "formatting"}},
                {"default_functions", {"write", "edit", "format"}}
            },
            {
                {"name", "reviewer_agent"},
                {"description", "Agent specialized in reviewing and critiquing content"},
                {"type", "reviewer"},
                {"capabilities", {"analysis", "critique", "feedback"}},
                {"default_functions", {"review", "critique", "suggest"}}
            },
            {
                {"name", "data_analyst"},
                {"description", "Agent specialized in data analysis and visualization"},
                {"type", "analyst"},
                {"capabilities", {"data_processing", "statistical_analysis", "visualization"}},
                {"default_functions", {"analyze_data", "create_charts", "generate_insights"}}
            },
            {
                {"name", "coordinator_agent"},
                {"description", "Agent specialized in coordinating other agents"},
                {"type", "coordinator"},
                {"capabilities", {"task_coordination", "workflow_management", "communication"}},
                {"default_functions", {"coordinate", "delegate", "monitor"}}
            }
        };
        
        json response = {
            {"templates", templates},
            {"count", templates.size()}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_create_agent_from_template(SocketType sock, const std::string& template_name, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto json_data = json::parse(body);
        
        // Create agent config based on template
        agents::AgentConfig config;
        config.name = json_data.value("name", template_name + "_instance");
        config.type = template_name;
        config.auto_start = json_data.value("auto_start", false);
        
        // Set template-specific defaults
        if (template_name == "research_agent") {
            config.llm_config.instruction = "You are a research agent specialized in gathering and analyzing information.";
            config.capabilities = {"web_search", "document_analysis", "summarization"};
        } else if (template_name == "writer_agent") {
            config.llm_config.instruction = "You are a writing agent specialized in creating high-quality content.";
            config.capabilities = {"text_generation", "editing", "formatting"};
        } else if (template_name == "reviewer_agent") {
            config.llm_config.instruction = "You are a reviewer agent specialized in analyzing and critiquing content.";
            config.capabilities = {"analysis", "critique", "feedback"};
        } else if (template_name == "data_analyst") {
            config.llm_config.instruction = "You are a data analyst agent specialized in processing and analyzing data.";
            config.capabilities = {"data_processing", "statistical_analysis", "visualization"};
        } else if (template_name == "coordinator_agent") {
            config.llm_config.instruction = "You are a coordinator agent specialized in managing workflows and other agents.";
            config.capabilities = {"task_coordination", "workflow_management", "communication"};
        } else {
            send_error_response(sock, 404, "Template not found");
            return;
        }
        
        // Override with user-provided configuration
        if (json_data.contains("llm_config")) {
            auto llm_config = json_data["llm_config"];
            if (llm_config.contains("model_name")) {
                config.llm_config.model_name = llm_config["model_name"];
            }
            if (llm_config.contains("temperature")) {
                config.llm_config.temperature = llm_config["temperature"];
            }
            if (llm_config.contains("max_tokens")) {
                config.llm_config.max_tokens = llm_config["max_tokens"];
            }
            if (llm_config.contains("system_prompt")) {
                config.llm_config.instruction = llm_config["system_prompt"];
            }
        }
        
        if (json_data.contains("capabilities")) {
            config.capabilities = json_data["capabilities"];
        }
        
        // Create the agent
        std::string agent_id = agent_manager->create_agent_from_config(config);
        
        if (agent_id.empty()) {
            send_error_response(sock, 500, "Failed to create agent from template");
            return;
        }
        
        // Auto-start if requested
        if (config.auto_start) {
            agent_manager->start_agent(agent_id);
        }
        
        json response;
        response["agent_id"] = agent_id;
        response["template"] = template_name;
        response["name"] = config.name;
        response["type"] = config.type;
        response["auto_started"] = config.auto_start;
        response["message"] = "Agent created successfully from template";
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Bulk operations
void AgentsRoute::handle_bulk_start_agents(SocketType sock, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto json_data = json::parse(body);
        
        if (!json_data.contains("agent_ids") || !json_data["agent_ids"].is_array()) {
            send_error_response(sock, 400, "Missing or invalid agent_ids array");
            return;
        }
        
        std::vector<std::string> agent_ids = json_data["agent_ids"];
        json results = json::array();
        int success_count = 0;
        
        for (const auto& agent_id : agent_ids) {
            bool success = agent_manager->start_agent(agent_id);
            results.push_back({
                {"agent_id", agent_id},
                {"success", success},
                {"message", success ? "Started successfully" : "Failed to start"}
            });
            if (success) success_count++;
        }
        
        json response = {
            {"total_agents", agent_ids.size()},
            {"successful_starts", success_count},
            {"failed_starts", agent_ids.size() - success_count},
            {"results", results}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_bulk_stop_agents(SocketType sock, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto json_data = json::parse(body);
        
        if (!json_data.contains("agent_ids") || !json_data["agent_ids"].is_array()) {
            send_error_response(sock, 400, "Missing or invalid agent_ids array");
            return;
        }
        
        std::vector<std::string> agent_ids = json_data["agent_ids"];
        json results = json::array();
        int success_count = 0;
        
        for (const auto& agent_id : agent_ids) {
            bool success = agent_manager->stop_agent(agent_id);
            results.push_back({
                {"agent_id", agent_id},
                {"success", success},
                {"message", success ? "Stopped successfully" : "Failed to stop"}
            });
            if (success) success_count++;
        }
        
        json response = {
            {"total_agents", agent_ids.size()},
            {"successful_stops", success_count},
            {"failed_stops", agent_ids.size() - success_count},
            {"results", results}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void AgentsRoute::handle_bulk_delete_agents(SocketType sock, const std::string& body) {
    try {
        if (!agent_manager) {
            send_error_response(sock, 503, "Agent manager not available");
            return;
        }
        
        auto json_data = json::parse(body);
        
        if (!json_data.contains("agent_ids") || !json_data["agent_ids"].is_array()) {
            send_error_response(sock, 400, "Missing or invalid agent_ids array");
            return;
        }
        
        std::vector<std::string> agent_ids = json_data["agent_ids"];
        json results = json::array();
        int success_count = 0;
        
        for (const auto& agent_id : agent_ids) {
            bool success = agent_manager->delete_agent(agent_id);
            results.push_back({
                {"agent_id", agent_id},
                {"success", success},
                {"message", success ? "Deleted successfully" : "Failed to delete"}
            });
            if (success) success_count++;
        }
        
        json response = {
            {"total_agents", agent_ids.size()},
            {"successful_deletions", success_count},
            {"failed_deletions", agent_ids.size() - success_count},
            {"results", results}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Utility methods
std::string AgentsRoute::format_error_response(const std::string& error, int code) {
    json response = {
        {"success", false},
        {"error", error},
        {"code", code}
    };
    return response.dump();
}

std::string AgentsRoute::format_success_response(const nlohmann::json& data) {
    json response = {
        {"success", true},
        {"data", data}
    };
    return response.dump();
}

bool AgentsRoute::validate_agent_config(const nlohmann::json& config) {
    try {
        // Basic validation - check required fields
        if (!config.contains("name") || config["name"].empty()) {
            return false;
        }
        
        if (config.contains("llm_config")) {
            auto llm_config = config["llm_config"];
            if (llm_config.contains("temperature")) {
                double temp = llm_config["temperature"];
                if (temp < 0.0 || temp > 2.0) {
                    return false;
                }
            }
            if (llm_config.contains("max_tokens")) {
                int tokens = llm_config["max_tokens"];
                if (tokens <= 0 || tokens > 100000) {
                    return false;
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error validating agent config: %s", e.what());
        return false;
    }
}

bool AgentsRoute::validate_message_payload(const nlohmann::json& payload) {
    try {
        if (!payload.contains("message") || payload["message"].empty()) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error validating message payload: %s", e.what());
        return false;
    }
}

nlohmann::json AgentsRoute::agent_to_json(const std::shared_ptr<agents::AgentCore>& agent) {
    json agent_json;
    agent_json["agent_id"] = agent->get_agent_id();
    agent_json["name"] = agent->get_agent_name();
    agent_json["type"] = agent->get_agent_type();
    agent_json["running"] = agent->is_running();
    
    try {
        agent_json["capabilities"] = agent->get_capabilities();
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting capabilities for agent %s: %s", 
            agent->get_agent_id().c_str(), e.what());
        agent_json["capabilities"] = json::array();
    }
    
    try {
        agent_json["functions"] = agent->get_function_manager()->get_function_names();
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting functions for agent %s: %s", 
            agent->get_agent_id().c_str(), e.what());
        agent_json["functions"] = json::array();
    }
    
    agent_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return agent_json;
}

nlohmann::json AgentsRoute::create_agent_metrics(const std::shared_ptr<agents::AgentCore>& agent) {
    json metrics;
    metrics["agent_id"] = agent->get_agent_id();
    metrics["running"] = agent->is_running();
    metrics["uptime_seconds"] = 0; // TODO: Calculate actual uptime
    metrics["request_count"] = 0; // TODO: Get actual request count
    metrics["error_count"] = 0; // TODO: Get actual error count
    metrics["average_response_time_ms"] = 0.0; // TODO: Calculate actual average response time
    metrics["memory_usage_mb"] = 0; // TODO: Get actual memory usage
    metrics["cpu_usage_percent"] = 0.0; // TODO: Get actual CPU usage
    metrics["last_activity"] = 0; // TODO: Get actual last activity timestamp
    
    return metrics;
}

} // namespace kolosal::routes

#include "kolosal/routes/api_dispatcher.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>
#include <sstream>

using json = nlohmann::json;

namespace kolosal::routes {

APIDispatcher::APIDispatcher(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager,
                           std::shared_ptr<agents::WorkflowEngine> engine)
    : agent_manager(manager), workflow_engine(engine) {
    
    // Initialize sub-routes
    if (agent_manager) {
        agents_route = std::make_unique<AgentsRoute>(agent_manager);
    }
    
    if (workflow_engine) {
        // Create a simple orchestrator for the orchestration route if needed
        // In a full implementation, this would be passed in
        orchestration_route = std::make_unique<OrchestrationRoute>(nullptr, workflow_engine);
    }
    
    ServerLogger::logInfo("APIDispatcher initialized with agent_manager: %s, workflow_engine: %s",
                         agent_manager ? "available" : "null",
                         workflow_engine ? "available" : "null");
}

bool APIDispatcher::match(const std::string& method, const std::string& path) {
    // Store current request context
    current_method = method;
    current_path = path;
    
    // Match all /api/v1/ paths for agents and orchestration
    bool matches = (path.find("/api/v1/agents") == 0 || 
                   path.find("/api/v1/orchestration") == 0);
    
    ServerLogger::logInfo("APIDispatcher::match - method: %s, path: %s, matches: %s",
                         method.c_str(), path.c_str(), matches ? "true" : "false");
    
    return matches;
}

void APIDispatcher::handle(SocketType sock, const std::string& body) {
    current_body = body;
    
    try {
        ServerLogger::logInfo("APIDispatcher::handle - dispatching %s %s", 
                             current_method.c_str(), current_path.c_str());
        
        if (current_path.find("/api/v1/agents") == 0) {
            handle_agent_endpoints(sock, current_method, current_path, current_body);
        } else if (current_path.find("/api/v1/orchestration") == 0) {
            handle_orchestration_endpoints(sock, current_method, current_path, current_body);
        } else {
            send_error_response(sock, 404, "API endpoint not found");
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in APIDispatcher::handle: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void APIDispatcher::handle_agent_endpoints(SocketType sock, const std::string& method, const std::string& path, const std::string& body) {
    ServerLogger::logInfo("Handling agent endpoint: %s %s", method.c_str(), path.c_str());
    
    try {
        // Parse agent endpoints
        if (path == "/api/v1/agents" && method == "POST") {
            // POST /api/v1/agents - Create new agent
            dispatch_agent_create(sock, body);
        } else if (path == "/api/v1/agents" && method == "GET") {
            // GET /api/v1/agents - List all agents
            dispatch_agent_list(sock);
        } else if (path == "/api/v1/agents/system/status" && method == "GET") {
            // GET /api/v1/agents/system/status - Agent system status
            dispatch_agent_system_status(sock);
        } else if (path == "/api/v1/agents/templates" && method == "GET") {
            // GET /api/v1/agents/templates - List agent templates
            dispatch_agent_templates_list(sock);
        } else if (path == "/api/v1/agents/bulk/start" && method == "POST") {
            // POST /api/v1/agents/bulk/start - Bulk start agents
            dispatch_agent_bulk_start(sock, body);
        } else if (path == "/api/v1/agents/bulk/stop" && method == "POST") {
            // POST /api/v1/agents/bulk/stop - Bulk stop agents
            dispatch_agent_bulk_stop(sock, body);
        } else if (path == "/api/v1/agents/bulk/delete" && method == "POST") {
            // POST /api/v1/agents/bulk/delete - Bulk delete agents
            dispatch_agent_bulk_delete(sock, body);
        } else {
            // Handle parameterized agent endpoints
            std::regex agent_pattern(R"(/api/v1/agents/([^/]+)(?:/([^/]+))?(?:/([^/]+))?)");
            std::regex template_pattern(R"(/api/v1/agents/templates/([^/]+))");
            std::smatch matches;
            
            if (std::regex_match(path, matches, template_pattern) && method == "POST") {
                // POST /api/v1/agents/templates/{template_name} - Create agent from template
                std::string template_name = matches[1].str();
                dispatch_agent_template_create(sock, template_name, body);
            } else if (std::regex_match(path, matches, agent_pattern)) {
                std::string agent_id = matches[1].str();
                std::string action = matches.size() > 2 ? matches[2].str() : "";
                std::string sub_action = matches.size() > 3 ? matches[3].str() : "";
                
                if (action.empty()) {
                    if (method == "GET") {
                        // GET /api/v1/agents/{agent_id} - Get agent details
                        dispatch_agent_get(sock, agent_id);
                    } else if (method == "PUT") {
                        // PUT /api/v1/agents/{agent_id} - Update agent
                        dispatch_agent_update(sock, agent_id, body);
                    } else if (method == "DELETE") {
                        // DELETE /api/v1/agents/{agent_id} - Delete agent
                        dispatch_agent_delete(sock, agent_id);
                    } else {
                        send_error_response(sock, 405, "Method not allowed");
                    }
                } else if (action == "start" && method == "POST") {
                    // POST /api/v1/agents/{agent_id}/start - Start agent
                    dispatch_agent_start(sock, agent_id);
                } else if (action == "stop" && method == "POST") {
                    // POST /api/v1/agents/{agent_id}/stop - Stop agent
                    dispatch_agent_stop(sock, agent_id);
                } else if (action == "restart" && method == "POST") {
                    // POST /api/v1/agents/{agent_id}/restart - Restart agent
                    dispatch_agent_restart(sock, agent_id);
                } else if (action == "status" && method == "GET") {
                    // GET /api/v1/agents/{agent_id}/status - Get agent status
                    dispatch_agent_status(sock, agent_id);
                } else if (action == "capabilities" && method == "GET") {
                    // GET /api/v1/agents/{agent_id}/capabilities - Get agent capabilities
                    dispatch_agent_capabilities(sock, agent_id);
                } else if (action == "functions" && method == "GET") {
                    // GET /api/v1/agents/{agent_id}/functions - List agent functions
                    dispatch_agent_functions(sock, agent_id);
                } else if (action == "functions" && !sub_action.empty()) {
                    if (method == "POST") {
                        // POST /api/v1/agents/{agent_id}/functions/{function_name} - Execute function
                        dispatch_agent_function_execute(sock, agent_id, sub_action, body);
                    } else if (method == "GET") {
                        // GET /api/v1/agents/{agent_id}/functions/{function_name} - Test function
                        dispatch_agent_function_test(sock, agent_id, sub_action, body);
                    } else {
                        send_error_response(sock, 405, "Method not allowed");
                    }
                } else {
                    send_error_response(sock, 404, "Agent endpoint not found");
                }
            } else {
                send_error_response(sock, 404, "Agent endpoint not found");
            }
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error handling agent endpoint: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void APIDispatcher::handle_orchestration_endpoints(SocketType sock, const std::string& method, const std::string& path, const std::string& body) {
    ServerLogger::logInfo("Handling orchestration endpoint: %s %s", method.c_str(), path.c_str());
    
    if (!workflow_engine) {
        send_error_response(sock, 503, "Workflow engine not available");
        return;
    }
    
    try {
        // Parse orchestration endpoints
        if (path == "/api/v1/orchestration/workflows" && method == "POST") {
            // POST /api/v1/orchestration/workflows - Create workflow
            dispatch_workflow_create(sock, body);
        } else if (path == "/api/v1/orchestration/workflows" && method == "GET") {
            // GET /api/v1/orchestration/workflows - List workflows
            dispatch_workflow_list(sock);
        } else if (path == "/api/v1/orchestration/workflows/history" && method == "GET") {
            // GET /api/v1/orchestration/workflows/history - Workflow execution history
            dispatch_workflow_history(sock);
        } else if (path == "/api/v1/orchestration/status" && method == "GET") {
            // GET /api/v1/orchestration/status - Orchestration system status
            dispatch_orchestration_status(sock);
        } else if (path == "/api/v1/orchestration/metrics" && method == "GET") {
            // GET /api/v1/orchestration/metrics - Workflow execution metrics
            dispatch_orchestration_metrics(sock);
        } else if (path == "/api/v1/orchestration/workflows/templates/sequential" && method == "POST") {
            // POST /api/v1/orchestration/workflows/templates/sequential - Create sequential workflow
            dispatch_workflow_template_sequential(sock, body);
        } else if (path == "/api/v1/orchestration/workflows/templates/parallel" && method == "POST") {
            // POST /api/v1/orchestration/workflows/templates/parallel - Create parallel workflow
            dispatch_workflow_template_parallel(sock, body);
        } else if (path == "/api/v1/orchestration/workflows/templates/pipeline" && method == "POST") {
            // POST /api/v1/orchestration/workflows/templates/pipeline - Create pipeline workflow
            dispatch_workflow_template_pipeline(sock, body);
        } else if (path == "/api/v1/orchestration/workflows/templates/consensus" && method == "POST") {
            // POST /api/v1/orchestration/workflows/templates/consensus - Create consensus workflow
            dispatch_workflow_template_consensus(sock, body);
        } else {
            // Handle parameterized orchestration endpoints
            std::regex workflow_pattern(R"(/api/v1/orchestration/workflows/([^/]+)(?:/([^/]+))?)");
            std::smatch matches;
            
            if (std::regex_match(path, matches, workflow_pattern)) {
                std::string workflow_id = matches[1].str();
                std::string action = matches.size() > 2 ? matches[2].str() : "";
                
                if (action.empty()) {
                    if (method == "GET") {
                        // GET /api/v1/orchestration/workflows/{workflow_id} - Get workflow details
                        dispatch_workflow_get(sock, workflow_id);
                    } else if (method == "PUT") {
                        // PUT /api/v1/orchestration/workflows/{workflow_id} - Update workflow
                        dispatch_workflow_update(sock, workflow_id, body);
                    } else if (method == "DELETE") {
                        // DELETE /api/v1/orchestration/workflows/{workflow_id} - Delete workflow
                        dispatch_workflow_delete(sock, workflow_id);
                    } else {
                        send_error_response(sock, 405, "Method not allowed");
                    }
                } else if (action == "execute" && method == "POST") {
                    // POST /api/v1/orchestration/workflows/{workflow_id}/execute - Execute workflow
                    dispatch_workflow_execute(sock, workflow_id, body);
                } else if (action == "status" && method == "GET") {
                    // GET /api/v1/orchestration/workflows/{workflow_id}/status - Workflow execution status
                    dispatch_workflow_status(sock, workflow_id);
                } else if (action == "history" && method == "GET") {
                    // GET /api/v1/orchestration/workflows/{workflow_id}/history - Workflow execution history
                    dispatch_workflow_history(sock, workflow_id);
                } else {
                    send_error_response(sock, 404, "Orchestration endpoint not found");
                }
            } else {
                send_error_response(sock, 404, "Orchestration endpoint not found");
            }
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error handling orchestration endpoint: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

// Agent management endpoint implementations
void APIDispatcher::dispatch_agent_create(SocketType sock, const std::string& body) {
    if (agents_route) {
        agents_route->handle_create_agent(sock, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_list(SocketType sock) {
    if (agents_route) {
        agents_route->handle_list_agents(sock);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_get(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_get_agent(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_update(SocketType sock, const std::string& agent_id, const std::string& body) {
    if (agents_route) {
        agents_route->handle_update_agent(sock, agent_id, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_delete(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_delete_agent(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_system_status(SocketType sock) {
    if (agents_route) {
        agents_route->handle_agent_system_status(sock);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_start(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_start_agent(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_stop(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_stop_agent(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_restart(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_restart_agent(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_status(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_agent_status(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_capabilities(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_get_agent_capabilities(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_functions(SocketType sock, const std::string& agent_id) {
    if (agents_route) {
        agents_route->handle_list_agent_functions(sock, agent_id);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_function_execute(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    if (agents_route) {
        agents_route->handle_execute_agent_function(sock, agent_id, function_name, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_function_test(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    if (agents_route) {
        agents_route->handle_test_agent_function(sock, agent_id, function_name, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_templates_list(SocketType sock) {
    if (agents_route) {
        agents_route->handle_list_agent_templates(sock);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_template_create(SocketType sock, const std::string& template_name, const std::string& body) {
    if (agents_route) {
        agents_route->handle_create_agent_from_template(sock, template_name, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_bulk_start(SocketType sock, const std::string& body) {
    if (agents_route) {
        agents_route->handle_bulk_start_agents(sock, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_bulk_stop(SocketType sock, const std::string& body) {
    if (agents_route) {
        agents_route->handle_bulk_stop_agents(sock, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

void APIDispatcher::dispatch_agent_bulk_delete(SocketType sock, const std::string& body) {
    if (agents_route) {
        agents_route->handle_bulk_delete_agents(sock, body);
    } else {
        send_error_response(sock, 503, "Agent management not available");
    }
}

// Workflow management endpoint implementations - simplified versions
void APIDispatcher::dispatch_workflow_create(SocketType sock, const std::string& body) {
    try {
        auto json_data = json::parse(body);
        
        // Basic validation
        if (!json_data.contains("name") || !json_data.contains("steps")) {
            send_error_response(sock, 400, "Missing required fields: name, steps");
            return;
        }
        
        // Create workflow using simplified workflow structure
        agents::Workflow workflow;
        workflow.name = json_data["name"];
        
        if (json_data.contains("description")) {
            workflow.description = json_data["description"];
        }
        
        if (json_data.contains("type")) {
            workflow.type = static_cast<agents::WorkflowType>(json_data["type"]);
        }
        
        // Add steps
        for (const auto& step_json : json_data["steps"]) {
            agents::WorkflowStep step;
            step.step_id = step_json["step_id"];
            step.name = step_json.value("name", "");
            step.agent_id = step_json["agent_id"];
            step.function_name = step_json["function_name"];
            
            if (step_json.contains("parameters")) {
                step.parameters = step_json["parameters"];
            }
            
            workflow.steps.push_back(step);
        }
        
        std::string workflow_id = workflow_engine->create_workflow(workflow);
        
        json response = {
            {"workflow_id", workflow_id},
            {"message", "Workflow created successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void APIDispatcher::dispatch_workflow_list(SocketType sock) {
    try {
        auto workflow_ids = workflow_engine->list_workflows();
        json workflows_array = json::array();
        
        for (const auto& workflow_id : workflow_ids) {
            auto workflow_opt = workflow_engine->get_workflow(workflow_id);
            if (workflow_opt) {
                json workflow_json;
                workflow_json["workflow_id"] = workflow_opt->workflow_id;
                workflow_json["name"] = workflow_opt->name;
                workflow_json["description"] = workflow_opt->description;
                workflow_json["type"] = static_cast<int>(workflow_opt->type);
                workflow_json["status"] = static_cast<int>(workflow_opt->status);
                workflow_json["steps_count"] = workflow_opt->steps.size();
                
                workflows_array.push_back(workflow_json);
            }
        }
        
        json response = {
            {"workflows", workflows_array},
            {"count", workflows_array.size()}
        };
        
        send_success_response(sock, response);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void APIDispatcher::dispatch_workflow_execute(SocketType sock, const std::string& workflow_id, const std::string& body) {
    try {
        json input_context = {};
        if (!body.empty()) {
            input_context = json::parse(body);
        }
        
        std::string execution_id = workflow_engine->execute_workflow(workflow_id, input_context);
        
        json response = {
            {"execution_id", execution_id},
            {"workflow_id", workflow_id},
            {"message", "Workflow execution started"}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void APIDispatcher::dispatch_orchestration_status(SocketType sock) {
    try {
        json status;
        status["engine_running"] = workflow_engine->is_running();
        status["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        auto active_executions = workflow_engine->get_active_executions();
        status["active_workflows"] = active_executions.size();
        
        auto metrics = workflow_engine->get_metrics();
        status["total_workflows"] = metrics.total_workflows;
        status["completed_workflows"] = metrics.completed_workflows;
        status["failed_workflows"] = metrics.failed_workflows;
        status["service_status"] = "healthy";
        
        send_success_response(sock, status);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

void APIDispatcher::dispatch_orchestration_metrics(SocketType sock) {
    try {
        auto metrics = workflow_engine->get_metrics();
        
        json metrics_json;
        metrics_json["total_workflows"] = metrics.total_workflows;
        metrics_json["running_workflows"] = metrics.running_workflows;
        metrics_json["completed_workflows"] = metrics.completed_workflows;
        metrics_json["failed_workflows"] = metrics.failed_workflows;
        metrics_json["cancelled_workflows"] = metrics.cancelled_workflows;
        metrics_json["average_execution_time_ms"] = metrics.average_execution_time_ms;
        metrics_json["success_rate"] = metrics.success_rate;
        
        send_success_response(sock, metrics_json);
        
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Simplified workflow template implementations
void APIDispatcher::dispatch_workflow_template_sequential(SocketType sock, const std::string& body) {
    try {
        auto json_data = json::parse(body);
        
        if (!json_data.contains("name") || !json_data.contains("agent_functions")) {
            send_error_response(sock, 400, "Missing required fields: name, agent_functions");
            return;
        }
        
        std::string name = json_data["name"];
        std::vector<std::pair<std::string, std::string>> agent_functions;
        
        for (const auto& item : json_data["agent_functions"]) {
            agent_functions.emplace_back(item["agent_id"], item["function_name"]);
        }
        
        auto workflow = workflow_engine->create_sequential_workflow(name, agent_functions);
        std::string workflow_id = workflow_engine->create_workflow(workflow);
        
        json response = {
            {"workflow_id", workflow_id},
            {"workflow_type", "sequential"},
            {"message", "Sequential workflow created successfully"}
        };
        
        send_success_response(sock, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Implement other dispatch methods with similar patterns...
void APIDispatcher::dispatch_workflow_get(SocketType sock, const std::string& workflow_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_update(SocketType sock, const std::string& workflow_id, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_delete(SocketType sock, const std::string& workflow_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_pause(SocketType sock, const std::string& execution_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_resume(SocketType sock, const std::string& execution_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_cancel(SocketType sock, const std::string& execution_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_status(SocketType sock, const std::string& workflow_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_execution_status(SocketType sock, const std::string& execution_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_history(SocketType sock, const std::string& workflow_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_template_parallel(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_template_pipeline(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_workflow_template_consensus(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void APIDispatcher::dispatch_active_workflows(SocketType sock) {
    send_error_response(sock, 501, "Not implemented yet");
}

// Helper methods
void APIDispatcher::send_response(SocketType sock, int status_code, const std::string& content) {
    ::send_response(sock, status_code, content);
}

void APIDispatcher::send_json_response(SocketType sock, int status_code, const nlohmann::json& data) {
    send_response(sock, status_code, data.dump());
}

void APIDispatcher::send_error_response(SocketType sock, int status_code, const std::string& error) {
    json response = {
        {"success", false},
        {"error", error},
        {"code", status_code}
    };
    send_json_response(sock, status_code, response);
}

void APIDispatcher::send_success_response(SocketType sock, const nlohmann::json& data) {
    json response = {
        {"success", true},
        {"data", data}
    };
    send_json_response(sock, 200, response);
}

std::string APIDispatcher::extract_path_parameter(const std::string& path, const std::string& pattern, int param_index) {
    std::regex regex_pattern(pattern);
    std::smatch matches;
    
    if (std::regex_match(path, matches, regex_pattern) && matches.size() > param_index) {
        return matches[param_index].str();
    }
    
    return "";
}

} // namespace kolosal::routes

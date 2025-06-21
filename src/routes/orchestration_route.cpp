#include "kolosal/routes/orchestration_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>
#include <sstream>

using json = nlohmann::json;

namespace kolosal::routes {

OrchestrationRoute::OrchestrationRoute(std::shared_ptr<agents::AgentOrchestrator> orch)
    : orchestrator(orch) {
}

void OrchestrationRoute::setup_routes(Server& server) {
    // This method exists for compatibility with AgentsRoute pattern
    // The OrchestrationRoute instance should be added directly to the server
    // This is typically called from outside, so we don't add ourselves here
}

bool OrchestrationRoute::match(const std::string& method, const std::string& path) {
    // Store current request context for use in handle()
    current_method = method;
    current_path = path;
    
    // Match all orchestration API paths
    return path.find("/api/v1/orchestration/") == 0;
}

void OrchestrationRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Use stored method and path from match()
        const std::string& method = current_method;
        const std::string& path = current_path;
        
        // Route to appropriate handler based on path
        if (path.find("/api/v1/orchestration/workflows") == 0) {
            handle_workflow_routes(sock, method, path, body);
        } else if (path.find("/api/v1/orchestration/collaboration-groups") == 0) {
            handle_collaboration_routes(sock, method, path, body);
        } else if (path.find("/api/v1/orchestration/coordinate") == 0 || 
                   path.find("/api/v1/orchestration/pipelines") == 0) {
            handle_coordination_routes(sock, method, path, body);
        } else if (path.find("/api/v1/orchestration/metrics") == 0 || 
                   path.find("/api/v1/orchestration/status") == 0 ||
                   path.find("/api/v1/orchestration/active-workflows") == 0) {
            handle_monitoring_routes(sock, method, path, body);
        } else if (path.find("/api/v1/orchestration/select-agent") == 0 ||
                   path.find("/api/v1/orchestration/distribute-workload") == 0 ||
                   path.find("/api/v1/orchestration/optimize") == 0) {
            handle_loadbalancing_routes(sock, method, path, body);
        } else {
            send_error_response(sock, 404, "Endpoint not found");
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in orchestration route: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

// Helper methods for HTTP response handling
void OrchestrationRoute::send_response(SocketType sock, int status_code, const std::string& content) {
    ::send_response(sock, status_code, content);
}

void OrchestrationRoute::send_json_response(SocketType sock, int status_code, const nlohmann::json& data) {
    send_response(sock, status_code, data.dump());
}

void OrchestrationRoute::send_error_response(SocketType sock, int status_code, const std::string& error) {
    json response = {
        {"success", false},
        {"error", error},
        {"code", status_code}
    };
    send_json_response(sock, status_code, response);
}

void OrchestrationRoute::send_success_response(SocketType sock, const nlohmann::json& data) {
    json response = {
        {"success", true},
        {"data", data}
    };
    send_json_response(sock, 200, response);
}

// Route handler implementations
void OrchestrationRoute::handle_workflow_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body) {
    if (path == "/api/v1/orchestration/workflows") {
        if (method == "POST") {
            handle_create_workflow(sock, body);
        } else if (method == "GET") {
            handle_list_workflows(sock);
        } else {
            send_error_response(sock, 405, "Method not allowed");
        }
    } else {
        // Handle workflow ID-based routes
        std::regex workflow_pattern(R"(/api/v1/orchestration/workflows/([^/]+))");
        std::regex execute_pattern(R"(/api/v1/orchestration/workflows/([^/]+)/execute)");
        std::regex execute_async_pattern(R"(/api/v1/orchestration/workflows/([^/]+)/execute-async)");
        std::regex result_pattern(R"(/api/v1/orchestration/workflows/([^/]+)/result)");
        std::regex cancel_pattern(R"(/api/v1/orchestration/workflows/([^/]+)/cancel)");
        
        std::smatch matches;
        
        if (std::regex_match(path, matches, execute_async_pattern) && method == "POST") {
            handle_execute_workflow_async(sock, matches[1], body);
        } else if (std::regex_match(path, matches, execute_pattern) && method == "POST") {
            handle_execute_workflow(sock, matches[1], body);
        } else if (std::regex_match(path, matches, result_pattern) && method == "GET") {
            handle_get_workflow_result(sock, matches[1]);
        } else if (std::regex_match(path, matches, cancel_pattern) && method == "POST") {
            handle_cancel_workflow(sock, matches[1]);
        } else if (std::regex_match(path, matches, workflow_pattern)) {
            if (method == "GET") {
                handle_get_workflow(sock, matches[1]);
            } else if (method == "DELETE") {
                handle_delete_workflow(sock, matches[1]);
            } else {
                send_error_response(sock, 405, "Method not allowed");
            }
        } else {
            send_error_response(sock, 404, "Workflow endpoint not found");
        }
    }
}

void OrchestrationRoute::handle_collaboration_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body) {
    if (path == "/api/v1/orchestration/collaboration-groups") {
        if (method == "POST") {
            handle_create_collaboration_group(sock, body);
        } else if (method == "GET") {
            handle_list_collaboration_groups(sock);
        } else {
            send_error_response(sock, 405, "Method not allowed");
        }
    } else {
        // Handle group ID-based routes
        std::regex group_pattern(R"(/api/v1/orchestration/collaboration-groups/([^/]+))");
        std::regex execute_pattern(R"(/api/v1/orchestration/collaboration-groups/([^/]+)/execute)");
        std::regex result_pattern(R"(/api/v1/orchestration/collaboration-groups/([^/]+)/result)");
        
        std::smatch matches;
        
        if (std::regex_match(path, matches, execute_pattern) && method == "POST") {
            handle_execute_collaboration(sock, matches[1], body);
        } else if (std::regex_match(path, matches, result_pattern) && method == "GET") {
            handle_get_collaboration_result(sock, matches[1]);
        } else if (std::regex_match(path, matches, group_pattern) && method == "DELETE") {
            handle_delete_collaboration_group(sock, matches[1]);
        } else {
            send_error_response(sock, 404, "Collaboration endpoint not found");
        }
    }
}

void OrchestrationRoute::handle_coordination_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body) {
    if (path == "/api/v1/orchestration/coordinate" && method == "POST") {
        handle_coordinate_agents(sock, body);
    } else if (path == "/api/v1/orchestration/pipelines" && method == "POST") {
        handle_setup_pipeline(sock, body);
    } else {
        std::regex pipeline_execute_pattern(R"(/api/v1/orchestration/pipelines/([^/]+)/execute)");
        std::smatch matches;
        
        if (std::regex_match(path, matches, pipeline_execute_pattern) && method == "POST") {
            handle_execute_pipeline(sock, matches[1], body);
        } else {
            send_error_response(sock, 404, "Coordination endpoint not found");
        }
    }
}

void OrchestrationRoute::handle_monitoring_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body) {
    if (method != "GET") {
        send_error_response(sock, 405, "Method not allowed");
        return;
    }
    
    if (path == "/api/v1/orchestration/metrics") {
        handle_get_orchestration_metrics(sock);
    } else if (path == "/api/v1/orchestration/status") {
        handle_get_orchestrator_status(sock);
    } else if (path == "/api/v1/orchestration/active-workflows") {
        handle_get_active_workflows(sock);
    } else {
        send_error_response(sock, 404, "Monitoring endpoint not found");
    }
}

void OrchestrationRoute::handle_loadbalancing_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body) {
    if (method != "POST") {
        send_error_response(sock, 405, "Method not allowed");
        return;
    }
    
    if (path == "/api/v1/orchestration/select-agent") {
        handle_select_optimal_agent(sock, body);
    } else if (path == "/api/v1/orchestration/distribute-workload") {
        handle_distribute_workload(sock, body);
    } else if (path == "/api/v1/orchestration/optimize") {
        handle_optimize_allocation(sock, body);
    } else {
        send_error_response(sock, 404, "Load balancing endpoint not found");
    }
}// Individual endpoint handlers
void OrchestrationRoute::handle_create_workflow(SocketType sock, const std::string& body) {
    try {
        json request_json = json::parse(body);
        
        if (!validate_workflow_definition(request_json)) {
            send_error_response(sock, 400, "Invalid workflow definition");
            return;
        }
        
        agents::AgentOrchestrator::Workflow workflow;
        workflow.workflow_id = request_json.value("workflow_id", "");
        workflow.name = request_json.value("name", "");
        workflow.description = request_json.value("description", "");
        workflow.auto_cleanup = request_json.value("auto_cleanup", true);
        workflow.max_execution_time_seconds = request_json.value("max_execution_time_seconds", 300);
          // Parse global context
        if (request_json.contains("global_context")) {
            for (auto& [key, value] : request_json["global_context"].items()) {
                agents::AgentData context_data;
                context_data.from_json(value);
                workflow.global_context[key] = context_data;
            }
        }
        
        // Parse workflow steps
        if (request_json.contains("steps")) {
            for (const auto& step_json : request_json["steps"]) {
                agents::AgentOrchestrator::WorkflowStep step;
                step.step_id = step_json.value("step_id", "");
                step.agent_id = step_json.value("agent_id", "");
                step.function_name = step_json.value("function_name", "");
                step.parallel_allowed = step_json.value("parallel_allowed", true);
                step.timeout_seconds = step_json.value("timeout_seconds", 30);
                step.max_retries = step_json.value("max_retries", 3);
                
                if (step_json.contains("dependencies")) {
                    step.dependencies = step_json["dependencies"];
                }
                
                if (step_json.contains("parameters")) {
                    step.parameters.from_json(step_json["parameters"]);
                }
                
                workflow.steps.push_back(step);
            }
        }
        
        if (workflow.workflow_id.empty()) {
            workflow.workflow_id = "workflow_" + std::to_string(std::time(nullptr));
        }
        
        bool success = orchestrator->register_workflow(workflow);
        
        if (!success) {
            send_error_response(sock, 500, "Failed to register workflow");
            return;
        }
        
        json response_data = {
            {"workflow_id", workflow.workflow_id},
            {"status", "registered"}
        };
        
        send_response(sock, 201, json({{"success", true}, {"data", response_data}}).dump());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error creating workflow: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void OrchestrationRoute::handle_list_workflows(SocketType sock) {
    try {
        auto workflow_ids = orchestrator->list_workflows();
        
        json workflows_json = json::array();
        for (const auto& workflow_id : workflow_ids) {
            json workflow_info = {
                {"workflow_id", workflow_id}
                // TODO: Add more workflow details if available
            };
            workflows_json.push_back(workflow_info);
        }
        
        json response_data = {
            {"workflows", workflows_json},
            {"count", workflows_json.size()}
        };
        
        send_success_response(sock, response_data);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error listing workflows: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void OrchestrationRoute::handle_execute_workflow(SocketType sock, const std::string& workflow_id, const std::string& body) {
    try {
        json request_json = json::parse(body);
        
        agents::AgentData input_context;
        if (request_json.contains("input_context")) {
            input_context.from_json(request_json["input_context"]);
        }
        
        bool success = orchestrator->execute_workflow(workflow_id, input_context);
        
        if (!success) {
            send_error_response(sock, 404, "Workflow not found or execution failed");
            return;
        }
        
        // Get the result immediately since it's synchronous
        auto result = orchestrator->get_workflow_result(workflow_id);
        
        json response_data = workflow_result_to_json(result);
        send_success_response(sock, response_data);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error executing workflow: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void OrchestrationRoute::handle_execute_workflow_async(SocketType sock, const std::string& workflow_id, const std::string& body) {
    try {
        json request_json = json::parse(body);
        
        agents::AgentData input_context;
        if (request_json.contains("input_context")) {
            input_context.from_json(request_json["input_context"]);
        }
        
        bool success = orchestrator->execute_workflow_async(workflow_id, input_context);
        
        if (!success) {
            send_error_response(sock, 404, "Workflow not found");
            return;
        }
        
        json response_data = {
            {"workflow_id", workflow_id},
            {"status", "queued"},
            {"message", "Workflow execution started asynchronously"}
        };
        
        send_response(sock, 202, json({{"success", true}, {"data", response_data}}).dump());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error executing workflow async: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void OrchestrationRoute::handle_get_workflow_result(SocketType sock, const std::string& workflow_id) {
    try {
        auto result = orchestrator->get_workflow_result(workflow_id);
        
        if (result.workflow_id.empty() || (!result.success && result.error_message == "Workflow result not found")) {
            send_error_response(sock, 404, "Workflow result not found");
            return;
        }
        
        json response_data = workflow_result_to_json(result);
        send_success_response(sock, response_data);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting workflow result: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void OrchestrationRoute::handle_create_collaboration_group(SocketType sock, const std::string& body) {
    try {
        json request_json = json::parse(body);
        
        if (!validate_collaboration_group(request_json)) {
            send_error_response(sock, 400, "Invalid collaboration group definition");
            return;
        }
        
        agents::AgentOrchestrator::CollaborationGroup group;
        group.group_id = request_json.value("group_id", "");
        group.name = request_json.value("name", "");
        group.pattern = parse_collaboration_pattern(request_json.value("pattern", "sequential"));
        group.consensus_threshold = request_json.value("consensus_threshold", 2);
        group.max_negotiation_rounds = request_json.value("max_negotiation_rounds", 5);
        
        if (request_json.contains("agent_ids")) {
            group.agent_ids = request_json["agent_ids"];
        }
        
        if (group.group_id.empty()) {
            group.group_id = "group_" + std::to_string(std::time(nullptr));
        }
        
        bool success = orchestrator->create_collaboration_group(group);
        
        if (!success) {
            send_error_response(sock, 500, "Failed to create collaboration group");
            return;
        }
        
        json response_data = {
            {"group_id", group.group_id},
            {"status", "created"}
        };
        
        send_response(sock, 201, json({{"success", true}, {"data", response_data}}).dump());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error creating collaboration group: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void OrchestrationRoute::handle_get_orchestration_metrics(SocketType sock) {
    try {
        auto metrics = orchestrator->get_orchestration_metrics();
        
        json metrics_json;
        for (const auto& [key, value] : metrics) {
            metrics_json[key] = value;
        }
        metrics_json["orchestrator_status"] = orchestrator->get_orchestrator_status();
        
        send_success_response(sock, metrics_json);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting orchestration metrics: %s", e.what());
        send_error_response(sock, 500, e.what());
    }
}

void OrchestrationRoute::handle_get_orchestrator_status(SocketType sock) {
    try {
        json status_data = {
            {"status", orchestrator->get_orchestrator_status()},
            {"is_running", orchestrator->is_running()}
        };
        
        send_success_response(sock, status_data);
    } catch (const std::exception& e) {
        send_error_response(sock, 500, e.what());
    }
}

// Placeholder implementations for remaining handlers
void OrchestrationRoute::handle_get_workflow(SocketType sock, const std::string& workflow_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_cancel_workflow(SocketType sock, const std::string& workflow_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_delete_workflow(SocketType sock, const std::string& workflow_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_list_collaboration_groups(SocketType sock) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_execute_collaboration(SocketType sock, const std::string& group_id, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_get_collaboration_result(SocketType sock, const std::string& group_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_delete_collaboration_group(SocketType sock, const std::string& group_id) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_coordinate_agents(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_setup_pipeline(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_execute_pipeline(SocketType sock, const std::string& pipeline_id, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_get_active_workflows(SocketType sock) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_select_optimal_agent(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_distribute_workload(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

void OrchestrationRoute::handle_optimize_allocation(SocketType sock, const std::string& body) {
    send_error_response(sock, 501, "Not implemented yet");
}

// Helper method implementations
bool OrchestrationRoute::validate_workflow_definition(const nlohmann::json& workflow) {
    return workflow.contains("name") && 
           workflow.contains("steps") && 
           workflow["steps"].is_array() &&
           !workflow["name"].get<std::string>().empty();
}

bool OrchestrationRoute::validate_collaboration_group(const nlohmann::json& group) {
    return group.contains("name") && 
           group.contains("agent_ids") && 
           group["agent_ids"].is_array() &&
           !group["name"].get<std::string>().empty() &&
           !group["agent_ids"].empty();
}

agents::AgentOrchestrator::CollaborationPattern OrchestrationRoute::parse_collaboration_pattern(const std::string& pattern) {
    if (pattern == "sequential") return agents::AgentOrchestrator::CollaborationPattern::SEQUENTIAL;
    if (pattern == "parallel") return agents::AgentOrchestrator::CollaborationPattern::PARALLEL;
    if (pattern == "pipeline") return agents::AgentOrchestrator::CollaborationPattern::PIPELINE;
    if (pattern == "consensus") return agents::AgentOrchestrator::CollaborationPattern::CONSENSUS;
    if (pattern == "hierarchy") return agents::AgentOrchestrator::CollaborationPattern::HIERARCHY;
    if (pattern == "negotiation") return agents::AgentOrchestrator::CollaborationPattern::NEGOTIATION;
    return agents::AgentOrchestrator::CollaborationPattern::SEQUENTIAL; // Default
}

nlohmann::json OrchestrationRoute::workflow_result_to_json(const agents::AgentOrchestrator::WorkflowResult& result) {
    json result_json;
    result_json["workflow_id"] = result.workflow_id;
    result_json["success"] = result.success;
    result_json["error_message"] = result.error_message;
    result_json["total_execution_time_ms"] = result.total_execution_time_ms;
    
    json step_results_json;
    for (const auto& [step_id, step_result] : result.step_results) {
        json step_json;
        step_json["success"] = step_result.success;
        step_json["error_message"] = step_result.error_message;
        step_json["execution_time_ms"] = step_result.execution_time_ms;
        step_json["llm_response"] = step_result.llm_response;
        step_json["result_data"] = step_result.result_data.to_json();
        step_results_json[step_id] = step_json;
    }
    result_json["step_results"] = step_results_json;
    
    return result_json;
}

} // namespace kolosal::routes

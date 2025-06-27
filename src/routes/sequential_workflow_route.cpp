#include "kolosal/routes/sequential_workflow_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/agents/agent_data.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/auto_setup_manager.hpp"
#include <json.hpp>
#include <regex>
#include <sstream>

using json = nlohmann::json;

namespace {
    // Helper function to convert AgentDataValue to JSON
    json agent_data_value_to_json(const kolosal::agents::AgentDataValue& value) {
        switch (value.type) {
            case kolosal::agents::AgentDataValue::STRING:
                return value.s_val;
            case kolosal::agents::AgentDataValue::INT:
                return value.i_val;
            case kolosal::agents::AgentDataValue::DOUBLE:
                return value.d_val;
            case kolosal::agents::AgentDataValue::BOOL:
                return value.b_val;
            case kolosal::agents::AgentDataValue::ARRAY_STRING:
                return value.arr_s_val;
            case kolosal::agents::AgentDataValue::OBJECT_DATA:
                if (value.obj_val) {
                    json obj;
                    for (const auto& [key, val] : *value.obj_val) {
                        obj[key] = agent_data_value_to_json(val);
                    }
                    return obj;
                }
                return json::object();
            case kolosal::agents::AgentDataValue::NONE:
            default:
                return nullptr;
        }
    }
}

namespace kolosal::routes {

SequentialWorkflowRoute::SequentialWorkflowRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
    workflow_executor = std::make_shared<agents::SequentialWorkflowExecutor>(manager);
    ServerLogger::logInfo("Sequential workflow route initialized");
}

bool SequentialWorkflowRoute::match(const std::string& method, const std::string& path) {
    // Store the method and path for use in handle()
    last_matched_method = method;
    last_matched_path = path;
    
    // Match various sequential workflow endpoints
    std::vector<std::string> patterns = {
        R"(^/api/v1/sequential-workflows/?$)",              // List/Create workflows
        R"(^/api/v1/sequential-workflows/[^/]+/?$)",        // Get/Delete specific workflow  
        R"(^/api/v1/sequential-workflows/[^/]+/execute/?$)", // Execute workflow
        R"(^/api/v1/sequential-workflows/[^/]+/execute-async/?$)", // Execute workflow async
        R"(^/api/v1/sequential-workflows/[^/]+/result/?$)",  // Get workflow result
        R"(^/api/v1/sequential-workflows/[^/]+/status/?$)",  // Get workflow status
        R"(^/api/v1/sequential-workflows/[^/]+/cancel/?$)",  // Cancel workflow
        R"(^/api/v1/sequential-workflows/executor/metrics/?$)", // Get executor metrics
        R"(^/api/v1/sequential-workflows/templates/?$)"      // Create from template
    };
    
    for (const auto& pattern : patterns) {
        if (std::regex_match(path, std::regex(pattern))) {
            return true;
        }
    }
    
    return false;
}

void SequentialWorkflowRoute::handle(SocketType sock, const std::string& body) {
    try {
        // We need to get the method and path from the server infrastructure
        // For now, we'll determine the action based on the content and path patterns
        // This is a temporary solution - ideally the method and path should be passed as parameters
        
        // Store the current path for routing decisions
        current_path = last_matched_path;
        current_method = last_matched_method;
        
        // Route to appropriate handler based on method and path
        if (current_method == "GET" && current_path == "/api/v1/sequential-workflows") {
            handle_list_workflows(sock);
        } else if (current_method == "POST" && current_path == "/api/v1/sequential-workflows") {
            handle_create_workflow(sock, body);
        } else if (current_method == "GET" && std::regex_match(current_path, std::regex(R"(/api/v1/sequential-workflows/([^/]+)/?)"))) {
            std::string workflow_id = extract_workflow_id_from_path(current_path);
            handle_get_workflow(sock, workflow_id);
        } else if (current_method == "DELETE" && std::regex_match(current_path, std::regex(R"(/api/v1/sequential-workflows/([^/]+)/?)"))) {
            std::string workflow_id = extract_workflow_id_from_path(current_path);
            handle_delete_workflow(sock, workflow_id);
        } else if (current_method == "POST" && std::regex_match(current_path, std::regex(R"(/api/v1/sequential-workflows/([^/]+)/execute/?)"))) {
            std::string workflow_id = extract_workflow_id_from_path(current_path);
            handle_execute_workflow(sock, workflow_id, body);
        } else if (current_method == "POST" && std::regex_match(current_path, std::regex(R"(/api/v1/sequential-workflows/([^/]+)/execute-async/?)"))) {
            std::string workflow_id = extract_workflow_id_from_path(current_path);
            handle_execute_workflow_async(sock, workflow_id, body);
        } else if (current_method == "GET" && std::regex_match(current_path, std::regex(R"(/api/v1/sequential-workflows/([^/]+)/result/?)"))) {
            std::string workflow_id = extract_workflow_id_from_path(current_path);
            handle_get_workflow_result(sock, workflow_id);
        } else if (current_method == "GET" && std::regex_match(current_path, std::regex(R"(/api/v1/sequential-workflows/([^/]+)/status/?)"))) {
            std::string workflow_id = extract_workflow_id_from_path(current_path);
            handle_get_workflow_status(sock, workflow_id);
        } else if (current_method == "POST" && std::regex_match(current_path, std::regex(R"(/api/v1/sequential-workflows/([^/]+)/cancel/?)"))) {
            std::string workflow_id = extract_workflow_id_from_path(current_path);
            handle_cancel_workflow(sock, workflow_id);
        } else if (current_method == "GET" && current_path == "/api/v1/sequential-workflows/executor/metrics") {
            handle_get_executor_metrics(sock);
        } else if (current_method == "POST" && current_path == "/api/v1/sequential-workflows/templates") {
            handle_create_workflow_from_template(sock, body);
        } else {
            send_response(sock, 404, format_error_response("Endpoint not found", 404));
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error handling sequential workflow request: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_list_workflows(SocketType sock) {
    try {
        auto workflow_ids = workflow_executor->list_workflows();
        
        json workflows_json = json::array();
        for (const auto& workflow_id : workflow_ids) {
            auto workflow_opt = workflow_executor->get_workflow(workflow_id);
            if (workflow_opt) {
                json workflow_info;
                workflow_info["workflow_id"] = workflow_opt->workflow_id;
                workflow_info["workflow_name"] = workflow_opt->workflow_name;
                workflow_info["description"] = workflow_opt->description;
                workflow_info["total_steps"] = workflow_opt->steps.size();
                
                // Get status
                auto status = workflow_executor->get_workflow_status(workflow_id);
                workflow_info["status"] = status;
                
                workflows_json.push_back(workflow_info);
            }
        }
        
        json response;
        response["success"] = true;
        response["data"] = workflows_json;
        response["count"] = workflows_json.size();
        
        send_response(sock, 200, response.dump());
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error listing sequential workflows: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_get_workflow(SocketType sock, const std::string& workflow_id) {
    try {
        if (!is_valid_workflow_id(workflow_id)) {
            send_response(sock, 400, format_error_response("Invalid workflow ID", 400));
            return;
        }
        
        auto workflow_opt = workflow_executor->get_workflow(workflow_id);
        if (!workflow_opt) {
            send_response(sock, 404, format_error_response("Workflow not found", 404));
            return;
        }
        
        json workflow_json = workflow_to_json(*workflow_opt);
        
        // Add current status
        auto status = workflow_executor->get_workflow_status(workflow_id);
        workflow_json["current_status"] = status;
        
        send_response(sock, 200, format_success_response(workflow_json));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting sequential workflow: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_create_workflow(SocketType sock, const std::string& body) {
    try {
        // The body parameter already contains the JSON body content
        if (body.empty()) {
            send_response(sock, 400, format_error_response("Request body is empty", 400));
            return;
        }
        
        json request_json = json::parse(body);
        
        // Automatically map agent names to UUIDs if they appear to be names rather than UUIDs
        try {
            auto& server_api = ServerAPI::instance();
            auto& auto_setup_manager = server_api.getAutoSetupManager();
            
            // Get agent name to UUID mapping
            auto agent_mapping = auto_setup_manager.get_agent_name_to_uuid_mapping();
            
            // Check and update agent_id fields in workflow steps
            if (request_json.contains("steps") && request_json["steps"].is_array()) {
                bool updated_any = false;
                
                for (auto& step : request_json["steps"]) {
                    if (step.contains("agent_id") && step["agent_id"].is_string()) {
                        std::string agent_id = step["agent_id"];
                        
                        // Check if this looks like a name rather than a UUID
                        // UUIDs are typically 36 characters with dashes
                        if (agent_id.length() != 36 || agent_id.find('-') == std::string::npos) {
                            // This looks like a name, try to map it to UUID
                            auto it = agent_mapping.find(agent_id);
                            if (it != agent_mapping.end()) {
                                step["agent_id"] = it->second;
                                ServerLogger::logInfo("Auto-mapped agent '%s' to UUID '%s' in workflow step", 
                                                    agent_id.c_str(), it->second.c_str());
                                updated_any = true;
                            } else {
                                ServerLogger::logWarning("Agent '%s' not found in mapping. Available agents: %s", 
                                                       agent_id.c_str(), 
                                                       [&agent_mapping]() {
                                                           std::string available;
                                                           for (const auto& pair : agent_mapping) {
                                                               if (!available.empty()) available += ", ";
                                                               available += pair.first;
                                                           }
                                                           return available;
                                                       }().c_str());
                                
                                // Return helpful error message
                                json error_response;
                                error_response["error"] = {
                                    {"message", "Agent '" + agent_id + "' not found"},
                                    {"type", "invalid_agent_error"},
                                    {"available_agents", [&agent_mapping]() {
                                        std::vector<std::string> names;
                                        for (const auto& pair : agent_mapping) {
                                            names.push_back(pair.first);
                                        }
                                        return names;
                                    }()}
                                };
                                send_response(sock, 400, error_response.dump());
                                return;
                            }
                        }
                    }
                }
                
                if (updated_any) {
                    ServerLogger::logInfo("✅ Workflow agent names automatically mapped to UUIDs");
                }
            }
            
        } catch (const std::exception& e) {
            ServerLogger::logWarning("Failed to perform automatic agent mapping: %s", e.what());
            // Continue with original workflow - maybe UUIDs were provided directly
        }
        
        // Parse workflow from JSON (now with mapped UUIDs)
        auto workflow = parse_workflow_from_json(request_json);
        
        // Register workflow
        if (workflow_executor->register_workflow(workflow)) {
            json response_data;
            response_data["workflow_id"] = workflow.workflow_id;
            response_data["message"] = "Workflow created successfully";
            response_data["auto_mapping_applied"] = true;
            
            send_response(sock, 201, format_success_response(response_data));
        } else {
            send_response(sock, 409, format_error_response("Workflow already exists or invalid", 409));
        }
        
    } catch (const json::parse_error& e) {
        ServerLogger::logError("JSON parse error: %s", e.what());
        send_response(sock, 400, format_error_response("Invalid JSON format", 400));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error creating sequential workflow: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_execute_workflow(SocketType sock, const std::string& workflow_id, const std::string& body) {
    try {
        if (!is_valid_workflow_id(workflow_id)) {
            send_response(sock, 400, format_error_response("Invalid workflow ID", 400));
            return;
        }
        
        // Parse input context if provided
        agents::AgentData input_context;
        
        if (!body.empty()) {
            try {
                json request_json = json::parse(body);
                if (request_json.contains("input_context")) {
                    // Convert JSON to AgentData
                    for (auto& [key, value] : request_json["input_context"].items()) {
                        if (value.is_string()) {
                            input_context.set(key, value.get<std::string>());
                        } else if (value.is_number_integer()) {
                            input_context.set(key, std::to_string(value.get<int>()));
                        } else if (value.is_number_float()) {
                            input_context.set(key, std::to_string(value.get<double>()));
                        } else if (value.is_boolean()) {
                            input_context.set(key, value.get<bool>() ? "true" : "false");
                        } else {
                            input_context.set(key, value.dump());
                        }
                    }
                }
            } catch (const json::parse_error&) {
                // Ignore JSON parse errors for input context - use empty context
            }
        }
        
        // Execute workflow synchronously
        auto result = workflow_executor->execute_workflow(workflow_id, input_context);
        
        json result_json = workflow_result_to_json(result);
        send_response(sock, 200, format_success_response(result_json));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error executing sequential workflow: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_execute_workflow_async(SocketType sock, const std::string& workflow_id, const std::string& body) {
    try {
        if (!is_valid_workflow_id(workflow_id)) {
            send_response(sock, 400, format_error_response("Invalid workflow ID", 400));
            return;
        }
        
        // Parse input context if provided (similar to synchronous execution)
        agents::AgentData input_context;
        
        if (!body.empty()) {
            try {
                json request_json = json::parse(body);
                if (request_json.contains("input_context")) {
                    for (auto& [key, value] : request_json["input_context"].items()) {
                        if (value.is_string()) {
                            input_context.set(key, value.get<std::string>());
                        } else {
                            input_context.set(key, value.dump());
                        }
                    }
                }
            } catch (const json::parse_error&) {
                // Ignore JSON parse errors
            }
        }
        
        // Execute workflow asynchronously
        std::string execution_id = workflow_executor->execute_workflow_async(workflow_id, input_context);
        
        json response_data;
        response_data["execution_id"] = execution_id;
        response_data["workflow_id"] = workflow_id;
        response_data["message"] = "Workflow execution started";
        
        send_response(sock, 202, format_success_response(response_data));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error executing sequential workflow async: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_get_workflow_result(SocketType sock, const std::string& workflow_id) {
    try {
        if (!is_valid_workflow_id(workflow_id)) {
            send_response(sock, 400, format_error_response("Invalid workflow ID", 400));
            return;
        }
        
        auto result_opt = workflow_executor->get_workflow_result(workflow_id);
        if (!result_opt) {
            send_response(sock, 404, format_error_response("Workflow result not found", 404));
            return;
        }
        
        json result_json = workflow_result_to_json(*result_opt);
        send_response(sock, 200, format_success_response(result_json));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting workflow result: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_get_workflow_status(SocketType sock, const std::string& workflow_id) {
    try {
        if (!is_valid_workflow_id(workflow_id)) {
            send_response(sock, 400, format_error_response("Invalid workflow ID", 400));
            return;
        }
        
        auto status = workflow_executor->get_workflow_status(workflow_id);
        
        if (status.find("status") == status.end() || status["status"] == "not_found") {
            send_response(sock, 404, format_error_response("Workflow not found", 404));
            return;
        }
        
        json status_json(status);
        send_response(sock, 200, format_success_response(status_json));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting workflow status: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_cancel_workflow(SocketType sock, const std::string& workflow_id) {
    try {
        if (!is_valid_workflow_id(workflow_id)) {
            send_response(sock, 400, format_error_response("Invalid workflow ID", 400));
            return;
        }
        
        bool cancelled = workflow_executor->cancel_workflow(workflow_id);
        
        json response_data;
        response_data["workflow_id"] = workflow_id;
        response_data["cancelled"] = cancelled;
        response_data["message"] = cancelled ? "Workflow cancellation requested" : "Workflow not found or already completed";
        
        send_response(sock, 200, format_success_response(response_data));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error cancelling workflow: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_delete_workflow(SocketType sock, const std::string& workflow_id) {
    try {
        if (!is_valid_workflow_id(workflow_id)) {
            send_response(sock, 400, format_error_response("Invalid workflow ID", 400));
            return;
        }
        
        bool deleted = workflow_executor->remove_workflow(workflow_id);
        
        if (deleted) {
            json response_data;
            response_data["workflow_id"] = workflow_id;
            response_data["message"] = "Workflow deleted successfully";
            
            send_response(sock, 200, format_success_response(response_data));
        } else {
            send_response(sock, 404, format_error_response("Workflow not found", 404));
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error deleting workflow: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_get_executor_metrics(SocketType sock) {
    try {
        auto metrics = workflow_executor->get_executor_metrics();
        
        json metrics_json(metrics);
        send_response(sock, 200, format_success_response(metrics_json));
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting executor metrics: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

void SequentialWorkflowRoute::handle_create_workflow_from_template(SocketType sock, const std::string& body) {
    try {
        // The body parameter already contains the JSON body content
        if (body.empty()) {
            send_response(sock, 400, format_error_response("Request body is empty", 400));
            return;
        }
        
        json request_json = json::parse(body);
        
        // Check if template is provided
        if (!request_json.contains("template")) {
            send_response(sock, 400, format_error_response("Template is required", 400));
            return;
        }
        
        // Create workflow from template
        auto workflow = parse_workflow_from_json(request_json["template"]);
        
        // Override workflow ID if provided
        if (request_json.contains("workflow_id")) {
            workflow.workflow_id = request_json["workflow_id"];
        }
        
        // Register workflow
        if (workflow_executor->register_workflow(workflow)) {
            json response_data;
            response_data["workflow_id"] = workflow.workflow_id;
            response_data["message"] = "Workflow created from template successfully";
            
            send_response(sock, 201, format_success_response(response_data));
        } else {
            send_response(sock, 409, format_error_response("Workflow already exists or invalid", 409));
        }
        
    } catch (const json::parse_error& e) {
        ServerLogger::logError("JSON parse error: %s", e.what());
        send_response(sock, 400, format_error_response("Invalid JSON format", 400));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error creating workflow from template: %s", e.what());
        send_response(sock, 500, format_error_response(e.what()));
    }
}

// Utility method implementations
std::string SequentialWorkflowRoute::format_success_response(const nlohmann::json& data) {
    json response;
    response["success"] = true;
    response["data"] = data;
    response["timestamp"] = std::time(nullptr);
    return response.dump();
}

std::string SequentialWorkflowRoute::format_error_response(const std::string& message, int error_code) {
    json response;
    response["success"] = false;
    response["error"] = message;
    response["error_code"] = error_code;
    response["timestamp"] = std::time(nullptr);
    return response.dump();
}

agents::SequentialWorkflow SequentialWorkflowRoute::parse_workflow_from_json(const nlohmann::json& json_data) {
    agents::SequentialWorkflow workflow;
    
    if (json_data.contains("workflow_id")) {
        workflow.workflow_id = json_data["workflow_id"];
    }
    
    if (json_data.contains("workflow_name")) {
        workflow.workflow_name = json_data["workflow_name"];
    }
    
    if (json_data.contains("description")) {
        workflow.description = json_data["description"];
    }
    
    if (json_data.contains("stop_on_failure")) {
        workflow.stop_on_failure = json_data["stop_on_failure"];
    }
    
    if (json_data.contains("max_execution_time_seconds")) {
        workflow.max_execution_time_seconds = json_data["max_execution_time_seconds"];
    }
    
    if (json_data.contains("global_context")) {
        // Convert JSON object to AgentData
        for (auto& [key, value] : json_data["global_context"].items()) {
            if (value.is_string()) {
                workflow.global_context.set(key, value.get<std::string>());
            } else {
                workflow.global_context.set(key, value.dump());
            }
        }
    }
    
    if (json_data.contains("steps")) {
        for (const auto& step_json : json_data["steps"]) {
            auto step = parse_step_from_json(step_json);
            workflow.steps.push_back(step);
        }
    }
    
    return workflow;
}

agents::SequentialWorkflowStep SequentialWorkflowRoute::parse_step_from_json(const nlohmann::json& json_step) {
    agents::SequentialWorkflowStep step;
    
    if (json_step.contains("step_id")) {
        step.step_id = json_step["step_id"];
    }
    
    if (json_step.contains("step_name")) {
        step.step_name = json_step["step_name"];
    }
    
    if (json_step.contains("description")) {
        step.description = json_step["description"];
    }
    
    if (json_step.contains("agent_id")) {
        step.agent_id = json_step["agent_id"];
    }
    
    if (json_step.contains("function_name")) {
        step.function_name = json_step["function_name"];
    }
    
    if (json_step.contains("timeout_seconds")) {
        step.timeout_seconds = json_step["timeout_seconds"];
    }
    
    if (json_step.contains("max_retries")) {
        step.max_retries = json_step["max_retries"];
    }
    
    if (json_step.contains("continue_on_failure")) {
        step.continue_on_failure = json_step["continue_on_failure"];
    }
    
    if (json_step.contains("parameters")) {
        // Convert JSON object to AgentData
        for (auto& [key, value] : json_step["parameters"].items()) {
            if (value.is_string()) {
                step.parameters.set(key, value.get<std::string>());
            } else {
                step.parameters.set(key, value.dump());
            }
        }
    }
    
    return step;
}

nlohmann::json SequentialWorkflowRoute::workflow_to_json(const agents::SequentialWorkflow& workflow) {
    json workflow_json;
    
    workflow_json["workflow_id"] = workflow.workflow_id;
    workflow_json["workflow_name"] = workflow.workflow_name;
    workflow_json["description"] = workflow.description;
    workflow_json["stop_on_failure"] = workflow.stop_on_failure;
    workflow_json["max_execution_time_seconds"] = workflow.max_execution_time_seconds;
    
    // Convert global context
    json global_context_json;
    for (const auto& [key, value] : workflow.global_context.get_data()) {
        global_context_json[key] = agent_data_value_to_json(value);
    }
    workflow_json["global_context"] = global_context_json;
    
    // Convert steps
    json steps_json = json::array();
    for (const auto& step : workflow.steps) {
        json step_json;
        step_json["step_id"] = step.step_id;
        step_json["step_name"] = step.step_name;
        step_json["description"] = step.description;
        step_json["agent_id"] = step.agent_id;
        step_json["function_name"] = step.function_name;
        step_json["timeout_seconds"] = step.timeout_seconds;
        step_json["max_retries"] = step.max_retries;
        step_json["continue_on_failure"] = step.continue_on_failure;
        
        // Convert parameters
        json parameters_json;
        for (const auto& [key, value] : step.parameters.get_data()) {
            parameters_json[key] = agent_data_value_to_json(value);
        }
        step_json["parameters"] = parameters_json;
        
        steps_json.push_back(step_json);
    }
    workflow_json["steps"] = steps_json;
    
    return workflow_json;
}

nlohmann::json SequentialWorkflowRoute::workflow_result_to_json(const agents::SequentialWorkflowResult& result) {
    json result_json;
    
    result_json["workflow_id"] = result.workflow_id;
    result_json["workflow_name"] = result.workflow_name;
    result_json["success"] = result.success;
    result_json["error_message"] = result.error_message;
    result_json["total_execution_time_ms"] = result.total_execution_time_ms;
    result_json["total_steps"] = result.total_steps;
    result_json["successful_steps"] = result.successful_steps;
    result_json["failed_steps"] = result.failed_steps;
    result_json["skipped_steps"] = result.skipped_steps;
    result_json["retried_steps"] = result.retried_steps;
    result_json["executed_steps"] = result.executed_steps;
    
    // Convert step results
    json step_results_json;
    for (const auto& [step_id, step_result] : result.step_results) {
        json step_result_json;
        step_result_json["success"] = step_result.success;
        step_result_json["error_message"] = step_result.error_message;
        step_result_json["execution_time_ms"] = step_result.execution_time_ms;
        
        // Convert result data
        json result_data_json;
        for (const auto& [key, value] : step_result.result_data.get_data()) {
            result_data_json[key] = agent_data_value_to_json(value);
        }
        step_result_json["result_data"] = result_data_json;
        
        step_results_json[step_id] = step_result_json;
    }
    result_json["step_results"] = step_results_json;
    
    // Convert step execution times
    result_json["step_execution_times"] = result.step_execution_times;
    
    // Convert step errors
    result_json["step_errors"] = result.step_errors;
    
    return result_json;
}

std::string SequentialWorkflowRoute::extract_workflow_id_from_path(const std::string& path) {
    std::regex pattern(R"(/api/v1/sequential-workflows/([^/]+))");
    std::smatch matches;
    if (std::regex_search(path, matches, pattern) && matches.size() > 1) {
        return matches[1].str();
    }
    return "";
}

bool SequentialWorkflowRoute::is_valid_workflow_id(const std::string& workflow_id) {
    return !workflow_id.empty() && workflow_id.length() <= 100 && 
           std::regex_match(workflow_id, std::regex(R"([a-zA-Z0-9_-]+)"));
}

} // namespace kolosal::routes

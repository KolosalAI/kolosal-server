#include "kolosal/routes/function_execution_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

FunctionExecutionRoute::FunctionExecutionRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
}

FunctionExecutionRoute::~FunctionExecutionRoute() = default;

bool FunctionExecutionRoute::match(const std::string& method, const std::string& path) {
    // Store current request context
    current_method = method;
    current_path = path;
    
    // Match function execution endpoints
    return (method == "POST" && (
        path.find("/api/v1/functions/") == 0 ||
        path.find("/api/v1/agents/") == 0 && path.find("/functions/") != std::string::npos
    ));
}

void FunctionExecutionRoute::handle(SocketType sock, const std::string& body) {
    try {
        const std::string& path = current_path;
        const std::string& method = current_method;
        
        // Determine if this is an async execution request
        bool is_async = path.find("/async") != std::string::npos;
        
        // Route to appropriate handler
        if (path.find("/api/v1/functions/") == 0 || 
            path.find("/api/v1/agents/") == 0) {
            handleFunctionExecution(sock, body, is_async);
        } else {
            sendErrorResponse(sock, 404, "Function execution endpoint not found");
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in function execution route: %s", e.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(e.what()));
    }
}

void FunctionExecutionRoute::handleFunctionExecution(SocketType sock, const std::string& body, bool is_async) {
    if (!agent_manager) {
        sendErrorResponse(sock, 503, "Agent manager not available");
        return;
    }
    
    try {
        // Parse the JSON body
        json request_data;
        try {
            request_data = json::parse(body);
        } catch (const json::parse_error& e) {
            sendErrorResponse(sock, 400, "Invalid JSON format");
            return;
        }
        
        // Extract required fields
        std::string agent_id;
        std::string function_name;
        json function_params = json::object();
        
        // Try to extract from URL path first
        agent_id = extractAgentId(current_path);
        function_name = extractFunctionName(current_path);
        
        // If not in path, try to extract from request body
        if (agent_id.empty() && request_data.contains("agent_id")) {
            agent_id = request_data["agent_id"];
        }
        if (function_name.empty() && request_data.contains("function_name")) {
            function_name = request_data["function_name"];
        }
        if (request_data.contains("parameters")) {
            function_params = request_data["parameters"];
        }
        
        if (agent_id.empty()) {
            sendErrorResponse(sock, 400, "Missing agent_id");
            return;
        }
        if (function_name.empty()) {
            sendErrorResponse(sock, 400, "Missing function_name");
            return;
        }
        
        // Get the agent
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            sendErrorResponse(sock, 404, "Agent not found");
            return;
        }
        
        // Prepare function execution data
        agents::AgentData execution_data;
        execution_data.set("function_name", function_name);
        execution_data.set("parameters", function_params.dump());
        execution_data.set("execution_mode", is_async ? "async" : "sync");
        
        // Execute the function
        auto result = agent->execute_function(function_name, function_params.dump().empty() ? agents::AgentData() : execution_data);
        
        // Prepare response
        json response_data;
        if (result.success) {
            response_data["success"] = true;
            response_data["function_name"] = function_name;
            response_data["agent_id"] = agent_id;
            response_data["execution_mode"] = is_async ? "async" : "sync";
            
            // Get the result data
            std::string result_str = result.result_data.get_string("result");
            if (!result_str.empty()) {
                try {
                    response_data["result"] = json::parse(result_str);
                } catch (const json::parse_error&) {
                    response_data["result"] = result_str;
                }
            } else {
                response_data["result"] = "Function executed successfully";
            }
            
            // Add execution metadata
            if (result.result_data.get_string("execution_time") != "") {
                response_data["execution_time"] = result.result_data.get_string("execution_time");
            }
            if (is_async && result.result_data.get_string("task_id") != "") {
                response_data["task_id"] = result.result_data.get_string("task_id");
                response_data["status"] = "submitted";
            }
            
            sendSuccessResponse(sock, response_data);
        } else {
            sendErrorResponse(sock, 400, result.error_message.empty() ? "Function execution failed" : result.error_message);
        }
    } catch (const std::exception& e) {
        sendErrorResponse(sock, 500, "Failed to execute function: " + std::string(e.what()));
    }
}

std::string FunctionExecutionRoute::extractAgentId(const std::string& path) {
    // Pattern: /api/v1/agents/{agent_id}/functions/{function_name}
    std::regex agent_pattern(R"(/api/v1/agents/([^/]+)/functions/)");
    std::smatch matches;
    if (std::regex_search(path, matches, agent_pattern)) {
        return matches[1].str();
    }
    return "";
}

std::string FunctionExecutionRoute::extractFunctionName(const std::string& path) {
    // Pattern: /api/v1/functions/{function_name} or /api/v1/agents/{agent_id}/functions/{function_name}
    std::regex function_pattern(R"(/functions/([^/]+))");
    std::smatch matches;
    if (std::regex_search(path, matches, function_pattern)) {
        return matches[1].str();
    }
    
    // Also check for direct function endpoint
    std::regex direct_pattern(R"(/api/v1/functions/([^/]+))");
    if (std::regex_search(path, matches, direct_pattern)) {
        return matches[1].str();
    }
    
    return "";
}

void FunctionExecutionRoute::sendErrorResponse(SocketType sock, int status, const std::string& message, 
                                             const std::string& error_type, const std::string& param) {
    json error_response = {
        {"success", false},
        {"error", {
            {"message", message},
            {"type", error_type},
            {"param", param}
        }}
    };
    
    std::string response_str = error_response.dump();
    send_response(sock, status, response_str);
}

void FunctionExecutionRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data) {
    std::string response_str = data.dump();
    send_response(sock, 200, response_str);
}

} // namespace kolosal::routes

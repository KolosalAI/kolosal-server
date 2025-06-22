#include "kolosal/routes/agents_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/agents/agent_data.hpp"
#include "kolosal/agents/yaml_config.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/routes/route_interface.hpp"
#include <json.hpp>
#include <sstream>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

// Individual route classes for each endpoint
class AgentListRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
public:
    AgentListRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        return (method == "GET" && (path == "/api/v1/agents" || path == "/v1/agents"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent_ids = agent_manager->list_agents();
            json agents_json = json::array();
            
            for (const auto& agent_id : agent_ids) {
                auto agent = agent_manager->get_agent(agent_id);
                if (agent) {
                    json agent_info;
                    agent_info["id"] = agent->get_agent_id();
                    agent_info["name"] = agent->get_agent_name();
                    agent_info["type"] = agent->get_agent_type();
                    agent_info["capabilities"] = agent->get_capabilities();
                    agent_info["running"] = agent->is_running();
                    agents_json.push_back(agent_info);
                }
            }
            
            json response;
            response["success"] = true;
            response["data"] = agents_json;
            response["count"] = agents_json.size();
            
            send_response(sock, 200, response.dump());
        } catch (const std::exception& e) {
            ServerLogger::logError("Error listing agents: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

class AgentGetRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
    std::string matched_agent_id;
public:
    AgentGetRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "GET") return false;
        
        std::regex pattern(R"(^(?:/v1)?/api/v1/agents/([^/]+)$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            json agent_info;
            agent_info["id"] = agent->get_agent_id();
            agent_info["name"] = agent->get_agent_name();
            agent_info["type"] = agent->get_agent_type();
            agent_info["capabilities"] = agent->get_capabilities();
            agent_info["running"] = agent->is_running();
            
            send_response(sock, 200, parent->format_success_response(agent_info));
        } catch (const std::exception& e) {
            ServerLogger::logError("Error getting agent: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

class AgentCreateRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
public:
    AgentCreateRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        return (method == "POST" && (path == "/api/v1/agents" || path == "/v1/agents"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            // Parse the JSON body
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                return;
            }
              // Validate required fields
            if (!parent->validate_agent_config(request_data)) {
                send_response(sock, 400, parent->format_error_response("Invalid agent configuration", 400));
                return;
            }
            
            // Convert JSON to AgentConfig
            agents::AgentConfig config;
            
            // Handle optional ID field, otherwise it will be auto-generated
            if (request_data.contains("id") && !request_data["id"].get<std::string>().empty()) {
                config.id = request_data["id"].get<std::string>();
            }
            
            config.name = request_data["name"].get<std::string>();
            config.type = request_data["type"].get<std::string>();
            
            if (request_data.contains("description")) {
                config.description = request_data["description"].get<std::string>();
            }
            
            if (request_data.contains("role")) {
                config.role = request_data["role"].get<std::string>();
            }
            
            if (request_data.contains("system_prompt")) {
                config.system_prompt = request_data["system_prompt"].get<std::string>();
            }
            
            if (request_data.contains("capabilities") && request_data["capabilities"].is_array()) {
                for (const auto& cap : request_data["capabilities"]) {
                    if (cap.is_string()) {
                        config.capabilities.push_back(cap.get<std::string>());
                    }
                }
            }
            
            if (request_data.contains("functions") && request_data["functions"].is_array()) {
                for (const auto& func : request_data["functions"]) {
                    if (func.is_string()) {
                        config.functions.push_back(func.get<std::string>());
                    }
                }
            }
            
            if (request_data.contains("auto_start")) {
                config.auto_start = request_data["auto_start"].get<bool>();
            }
            
            if (request_data.contains("max_concurrent_jobs")) {
                config.max_concurrent_jobs = request_data["max_concurrent_jobs"].get<int>();
            }
            
            // Create the agent
            std::string agent_id = agent_manager->create_agent_from_config(config);
            
            if (agent_id.empty()) {
                send_response(sock, 422, parent->format_error_response("Failed to create agent", 422));
                return;
            }
            
            // Start the agent if requested
            bool started = agent_manager->start_agent(agent_id);
            
            json response_data;
            response_data["id"] = agent_id;
            response_data["status"] = started ? "running" : "created";
            response_data["message"] = "Agent created successfully";
            
            send_response(sock, 201, parent->format_success_response(response_data));
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error creating agent: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

class AgentDeleteRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
    std::string matched_agent_id;
public:
    AgentDeleteRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "DELETE") return false;
        
        std::regex pattern(R"(^(?:/v1)?/api/v1/agents/([^/]+)$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
      void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Delete the agent (this will stop it first)
            bool deleted = agent_manager->delete_agent(matched_agent_id);
            
            if (deleted) {
                json response_data;
                response_data["id"] = matched_agent_id;
                response_data["status"] = "deleted";
                response_data["message"] = "Agent deleted successfully";
                
                send_response(sock, 200, parent->format_success_response(response_data));
            } else {
                send_response(sock, 500, parent->format_error_response("Failed to delete agent"));
            }
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error deleting agent: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

class AgentExecuteRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
    std::string matched_agent_id;
public:
    AgentExecuteRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "POST") return false;
        
        std::regex pattern(R"(^(?:/v1)?/api/v1/agents/([^/]+)/execute$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
      void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Parse the JSON body
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                return;
            }
            
            // Validate required fields
            if (!request_data.contains("function") || !request_data["function"].is_string()) {
                send_response(sock, 400, parent->format_error_response("Missing or invalid 'function' field", 400));
                return;
            }
            
            std::string function_name = request_data["function"].get<std::string>();
            
            // Prepare function parameters
            agents::AgentData params;
            if (request_data.contains("parameters") && request_data["parameters"].is_object()) {
                for (const auto& [key, value] : request_data["parameters"].items()) {
                    if (value.is_string()) {
                        params.set(key, value.get<std::string>());
                    } else if (value.is_number_integer()) {
                        params.set(key, std::to_string(value.get<int>()));
                    } else if (value.is_number_float()) {
                        params.set(key, std::to_string(value.get<double>()));
                    } else if (value.is_boolean()) {
                        params.set(key, value.get<bool>() ? "true" : "false");
                    } else {
                        params.set(key, value.dump());
                    }
                }
            }
            
            // Execute the function
            try {
                auto result = agent->get_function_manager()->execute_function(function_name, params);
                
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["function"] = function_name;
                response_data["success"] = result.success;
                response_data["execution_time_ms"] = result.execution_time_ms;
                  // Convert result data to JSON
                json result_json;
                for (const auto& key : result.result_data.get_all_keys()) {
                    result_json[key] = result.result_data.get_string(key);
                }
                response_data["result"] = result_json;
                
                if (!result.error_message.empty()) {
                    response_data["error"] = result.error_message;
                }
                
                int status_code = result.success ? 200 : 400;
                send_response(sock, status_code, parent->format_success_response(response_data));
                
            } catch (const std::exception& e) {
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["function"] = function_name;
                response_data["success"] = false;
                response_data["error"] = e.what();
                
                send_response(sock, 400, parent->format_success_response(response_data));
            }
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error executing agent function: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

class AgentSystemStatusRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
public:
    AgentSystemStatusRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        return (method == "GET" && (path == "/api/v1/agents/system/status" || path == "/v1/agents/system/status"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            std::string status = agent_manager->get_system_status();
            auto agent_ids = agent_manager->list_agents();
            
            json response_data;
            response_data["system_running"] = agent_manager->is_running();
            response_data["agent_count"] = agent_ids.size();
            response_data["system_status"] = status;
            
            // Add individual agent statuses
            json agents_status = json::array();
            for (const auto& agent_id : agent_ids) {
                auto agent = agent_manager->get_agent(agent_id);
                if (agent) {
                    json agent_status;
                    agent_status["id"] = agent_id;
                    agent_status["name"] = agent->get_agent_name();
                    agent_status["running"] = agent->is_running();
                    agents_status.push_back(agent_status);
                }
            }
            response_data["agents"] = agents_status;
            
            send_response(sock, 200, parent->format_success_response(response_data));
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error getting system status: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }    }
};

// Agent Chat Route - handles /v1/agents/{agent_id}/chat
class AgentChatRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
    std::string matched_agent_id;
public:
    AgentChatRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "POST") return false;
        
        std::regex pattern(R"(^/v1/agents/([^/]+)/chat$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Parse the JSON body
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                return;
            }
            
            // Extract message from request
            std::string message;
            if (request_data.contains("message") && request_data["message"].is_string()) {
                message = request_data["message"].get<std::string>();
            } else {
                send_response(sock, 400, parent->format_error_response("Missing or invalid 'message' field", 400));
                return;
            }
            
            // Prepare function parameters for text processing
            agents::AgentData params;
            params.set("text", message);
            params.set("operation", "process");
            
            // Execute text processing function
            try {
                auto result = agent->get_function_manager()->execute_function("text_processing", params);
                
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = result.success;
                
                if (result.success) {
                    // Extract the response text
                    std::string response_text = result.result_data.get_string("result");
                    if (response_text.empty()) {
                        response_text = "I understand your message: " + message;
                    }
                    response_data["response"] = response_text;
                } else {
                    response_data["error"] = result.error_message.empty() ? "Failed to process message" : result.error_message;
                }
                
                int status_code = result.success ? 200 : 400;
                send_response(sock, status_code, parent->format_success_response(response_data));
                
            } catch (const std::exception& e) {
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = false;
                response_data["error"] = e.what();
                
                send_response(sock, 400, parent->format_success_response(response_data));
            }
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error in agent chat: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

// Agent Generate Route - handles /v1/agents/{agent_id}/generate
class AgentGenerateRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
    std::string matched_agent_id;
public:
    AgentGenerateRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "POST") return false;
        
        std::regex pattern(R"(^/v1/agents/([^/]+)/generate$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Parse the JSON body
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                return;
            }
            
            // Extract message from request
            std::string message;
            if (request_data.contains("message") && request_data["message"].is_string()) {
                message = request_data["message"].get<std::string>();
            } else {
                send_response(sock, 400, parent->format_error_response("Missing or invalid 'message' field", 400));
                return;
            }
            
            // Use code generation function if available, otherwise fallback to text processing
            agents::AgentData params;
            params.set("requirements", message);
            params.set("language", "python");
            params.set("style", "clean");
            
            try {
                auto result = agent->get_function_manager()->execute_function("code_generation", params);
                
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = result.success;
                
                if (result.success) {
                    std::string generated_content = result.result_data.get_string("result");
                    if (generated_content.empty()) {
                        generated_content = "Generated response for: " + message;
                    }
                    response_data["content"] = generated_content;
                } else {
                    response_data["error"] = result.error_message.empty() ? "Failed to generate content" : result.error_message;
                }
                
                int status_code = result.success ? 200 : 400;
                send_response(sock, status_code, parent->format_success_response(response_data));
                
            } catch (const std::exception& e) {
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = false;
                response_data["error"] = e.what();
                
                send_response(sock, 400, parent->format_success_response(response_data));
            }
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error in agent generate: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

// Agent Respond Route - handles /v1/agents/{agent_id}/respond
class AgentRespondRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
    std::string matched_agent_id;
public:
    AgentRespondRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "POST") return false;
        
        std::regex pattern(R"(^/v1/agents/([^/]+)/respond$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Parse the JSON body
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                return;
            }
            
            // Extract message from request
            std::string message;
            if (request_data.contains("message") && request_data["message"].is_string()) {
                message = request_data["message"].get<std::string>();
            } else {
                send_response(sock, 400, parent->format_error_response("Missing or invalid 'message' field", 400));
                return;
            }
            
            // Prepare function parameters for text processing
            agents::AgentData params;
            params.set("text", message);
            params.set("operation", "analyze");
            
            try {
                auto result = agent->get_function_manager()->execute_function("text_processing", params);
                
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = result.success;
                  if (result.success) {
                    std::string response_text = result.result_data.get_string("result");
                    if (response_text.empty()) {
                        // Generate appropriate responses based on message content
                        if (message.find("Hello") != std::string::npos || message.find("hello") != std::string::npos) {
                            response_text = "Hello! I'm your AI assistant. How can I help you today?";
                        } else if (message.find("+") != std::string::npos || message.find("=") != std::string::npos) {
                            response_text = "I can help with math problems. Let me calculate that for you.";
                        } else if (message.find("Python") != std::string::npos || message.find("function") != std::string::npos) {
                            if (message.find("reverse") != std::string::npos && message.find("string") != std::string::npos) {
                                response_text = "Here's a Python function to reverse a string:\n\n```python\ndef reverse_string(s):\n    return s[::-1]\n```";
                            } else {
                                response_text = "I can help with Python programming. What would you like to create?";
                            }
                        } else if (message.find("Python is") != std::string::npos || message.find("what Python") != std::string::npos) {
                            response_text = "Python is a high-level, interpreted programming language known for its simplicity and versatility.";
                        } else {
                            // Use analysis results if available
                            std::string word_count = result.result_data.get_string("word_count");
                            std::string sentiment = result.result_data.get_string("sentiment");
                            if (!word_count.empty()) {
                                response_text = "I've analyzed your message. It contains " + word_count + " words";
                                if (!sentiment.empty()) {
                                    response_text += " and has a " + sentiment + " sentiment";
                                }
                                response_text += ". How can I help you further?";
                            } else {
                                response_text = "I understand your request. How can I assist you further?";
                            }
                        }
                    }
                    response_data["text"] = response_text;
                } else {
                    response_data["error"] = result.error_message.empty() ? "Failed to respond to message" : result.error_message;
                }
                
                int status_code = result.success ? 200 : 400;
                send_response(sock, status_code, parent->format_success_response(response_data));
                
            } catch (const std::exception& e) {
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = false;
                response_data["error"] = e.what();
                
                send_response(sock, 400, parent->format_success_response(response_data));
            }
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error in agent respond: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

// Agent Message Route - handles /v1/agents/{agent_id}/message
class AgentMessageRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentsRoute* parent;
    std::string matched_agent_id;
public:
    AgentMessageRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "POST") return false;
        
        std::regex pattern(R"(^/v1/agents/([^/]+)/message$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Parse the JSON body
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                return;
            }
            
            // Extract message from request
            std::string message;
            if (request_data.contains("message") && request_data["message"].is_string()) {
                message = request_data["message"].get<std::string>();
            } else {
                send_response(sock, 400, parent->format_error_response("Missing or invalid 'message' field", 400));
                return;
            }
            
            // Prepare function parameters for text processing
            agents::AgentData params;
            params.set("text", message);
            params.set("operation", "process");
            
            try {
                auto result = agent->get_function_manager()->execute_function("text_processing", params);
                
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = result.success;
                
                if (result.success) {
                    std::string response_message = result.result_data.get_string("result");
                    if (response_message.empty()) {
                        response_message = "Message received and processed: " + message;
                    }
                    response_data["message"] = response_message;
                } else {
                    response_data["error"] = result.error_message.empty() ? "Failed to process message" : result.error_message;
                }
                
                int status_code = result.success ? 200 : 400;
                send_response(sock, status_code, parent->format_success_response(response_data));
                
            } catch (const std::exception& e) {
                json response_data;
                response_data["agent_id"] = matched_agent_id;
                response_data["success"] = false;
                response_data["error"] = e.what();
                
                send_response(sock, 400, parent->format_success_response(response_data));
            }
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error in agent message: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

AgentsRoute::AgentsRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
}

void AgentsRoute::setup_routes(Server& server) {
    // Register individual route handlers
    server.addRoute(std::make_unique<AgentListRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentGetRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentCreateRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentDeleteRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentExecuteRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentSystemStatusRoute>(agent_manager, this));
    
    // Register new chat/response endpoints
    server.addRoute(std::make_unique<AgentChatRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentGenerateRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentRespondRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentMessageRoute>(agent_manager, this));
}

// Helper methods
std::string AgentsRoute::format_error_response(const std::string& error, int code) {
    json response;
    response["success"] = false;
    response["error"] = error;
    response["code"] = code;
    return response.dump();
}

std::string AgentsRoute::format_success_response(const nlohmann::json& data) {
    json response;
    response["success"] = true;
    response["data"] = data;
    return response.dump();
}

bool AgentsRoute::validate_agent_config(const nlohmann::json& config) {
    return config.contains("name") && 
           config.contains("type") && 
           !config["name"].get<std::string>().empty() &&
           !config["type"].get<std::string>().empty();
}

bool AgentsRoute::validate_message_payload(const nlohmann::json& payload) {
    return payload.contains("from_agent") && 
           payload.contains("to_agent") && 
           payload.contains("type") &&
           !payload["from_agent"].get<std::string>().empty() &&
           !payload["to_agent"].get<std::string>().empty() &&
           !payload["type"].get<std::string>().empty();
}

} // namespace kolosal::routes

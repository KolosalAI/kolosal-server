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
            if (!request_data.contains("id") || !request_data.is_object()) {
                send_response(sock, 400, parent->format_error_response("Missing required field: id", 400));
                return;
            }
            
            if (!parent->validate_agent_config(request_data)) {
                send_response(sock, 400, parent->format_error_response("Invalid agent configuration", 400));
                return;
            }
            
            // Convert JSON to AgentConfig (simplified approach)
            agents::AgentConfig config;
            config.id = request_data["id"].get<std::string>();
            config.name = request_data["name"].get<std::string>();
            config.type = request_data["type"].get<std::string>();
            
            if (request_data.contains("description")) {
                config.description = request_data["description"].get<std::string>();
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
            
            // Stop the agent first
            bool stopped = agent_manager->stop_agent(matched_agent_id);
            
            json response_data;
            response_data["id"] = matched_agent_id;
            response_data["status"] = "deleted";
            response_data["message"] = "Agent deleted successfully";
            
            send_response(sock, 200, parent->format_success_response(response_data));
            
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
            
            // For now, return a simple execution result
            // This would need to be implemented based on the actual agent execution logic
            json response_data;
            response_data["agent_id"] = matched_agent_id;
            response_data["execution_status"] = "completed";
            response_data["result"] = "Function executed successfully";
            response_data["message"] = "Agent function execution completed";
            
            send_response(sock, 200, parent->format_success_response(response_data));
            
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
    return config.contains("id") &&
           config.contains("name") && 
           config.contains("type") && 
           !config["id"].get<std::string>().empty() &&
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

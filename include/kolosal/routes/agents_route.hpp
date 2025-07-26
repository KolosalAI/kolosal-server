#pragma once

#include "../export.hpp"
#include "../server.hpp"
#include "../agents/multi_agent_system.hpp"
#include <memory>
#include <json.hpp>

namespace kolosal::routes {

/**
 * @brief Comprehensive agent management routes
 */
class KOLOSAL_SERVER_API AgentsRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;

public:
    explicit AgentsRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager);
    
    void setup_routes(Server& server);

    // Core agent management API endpoints
    void handle_create_agent(SocketType sock, const std::string& body);
    void handle_list_agents(SocketType sock);
    void handle_get_agent(SocketType sock, const std::string& agent_id);
    void handle_update_agent(SocketType sock, const std::string& agent_id, const std::string& body);
    void handle_delete_agent(SocketType sock, const std::string& agent_id);
    void handle_agent_system_status(SocketType sock);
    
    // Agent lifecycle management
    void handle_start_agent(SocketType sock, const std::string& agent_id);
    void handle_stop_agent(SocketType sock, const std::string& agent_id);
    void handle_restart_agent(SocketType sock, const std::string& agent_id);
    void handle_agent_status(SocketType sock, const std::string& agent_id);
    
    // Agent capabilities and functions
    void handle_get_agent_capabilities(SocketType sock, const std::string& agent_id);
    void handle_list_agent_functions(SocketType sock, const std::string& agent_id);
    void handle_execute_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body);
    void handle_test_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body);
    
    // Agent templates and presets
    void handle_list_agent_templates(SocketType sock);
    void handle_create_agent_from_template(SocketType sock, const std::string& template_name, const std::string& body);
    
    // Bulk operations
    void handle_bulk_start_agents(SocketType sock, const std::string& body);
    void handle_bulk_stop_agents(SocketType sock, const std::string& body);
    void handle_bulk_delete_agents(SocketType sock, const std::string& body);

    // Helper methods
    std::string format_error_response(const std::string& error, int code = 500);
    std::string format_success_response(const nlohmann::json& data = {});
    bool validate_agent_config(const nlohmann::json& config);
    bool validate_message_payload(const nlohmann::json& payload);
    nlohmann::json agent_to_json(const std::shared_ptr<agents::AgentCore>& agent);
    nlohmann::json create_agent_metrics(const std::shared_ptr<agents::AgentCore>& agent);
    
private:
    void send_response(SocketType sock, int status_code, const std::string& content);
    void send_json_response(SocketType sock, int status_code, const nlohmann::json& data);
    void send_error_response(SocketType sock, int status_code, const std::string& error);
    void send_success_response(SocketType sock, const nlohmann::json& data = {});
};

} // namespace kolosal::routes

#pragma once

#include "../route_interface.hpp"
#include "../../export.hpp"
#include "../../server.hpp"
#include <memory>
#include <json.hpp>

namespace kolosal::routes::agents {

/**
 * @brief Agent management route handler
 * 
 * This route implements the agent management endpoints:
 * - GET /api/v1/agents - List all agents
 * - POST /api/v1/agents - Create a new agent
 * - GET /api/v1/agents/{id} - Get agent details
 * - PUT /api/v1/agents/{id} - Update agent
 * - DELETE /api/v1/agents/{id} - Delete agent
 * - POST /api/v1/agents/{id}/start - Start agent
 * - POST /api/v1/agents/{id}/stop - Stop agent
 * - GET /api/v1/agents/{id}/status - Get agent status
 */
class KOLOSAL_SERVER_API AgentsRoute : public IRoute {
public:
    AgentsRoute();
    ~AgentsRoute() override = default;
    
    // IRoute interface implementation
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    // Core agent management API endpoints
    void handle_list_agents(SocketType sock);
    void handle_create_agent(SocketType sock, const std::string& body);
    void handle_get_agent(SocketType sock, const std::string& agent_id);
    void handle_update_agent(SocketType sock, const std::string& agent_id, const std::string& body);
    void handle_delete_agent(SocketType sock, const std::string& agent_id);
    
    // Agent lifecycle management
    void handle_start_agent(SocketType sock, const std::string& agent_id);
    void handle_stop_agent(SocketType sock, const std::string& agent_id);
    void handle_agent_status(SocketType sock, const std::string& agent_id);
    
    // Helper methods
    void send_response(SocketType sock, int status_code, const std::string& content);
    void send_json_response(SocketType sock, int status_code, const nlohmann::json& data);
    void send_error_response(SocketType sock, int status_code, const std::string& error);
    void send_success_response(SocketType sock, const nlohmann::json& data = {});
    
    // Route parsing helpers
    std::string current_method_;
    std::string current_path_;
    std::string extractIdFromPath(const std::string& path, const std::string& base_pattern);
    bool validate_agent_config(const nlohmann::json& config);
};

} // namespace kolosal::routes::agents

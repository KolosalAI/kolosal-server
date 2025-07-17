#pragma once

#include "route_interface.hpp"
#include "../agents/multi_agent_system.hpp"
#include "../agents/agent_orchestrator.hpp"
#include <memory>

namespace kolosal {

/**
 * @brief Route for agent system monitoring, health checks, and metrics
 */
class AgentMonitoringRoute : public IRoute {
public:
    explicit AgentMonitoringRoute(
        std::shared_ptr<agents::YAMLConfigurableAgentManager> manager,
        std::shared_ptr<agents::AgentOrchestrator> orchestrator = nullptr
    );

    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    std::shared_ptr<agents::AgentOrchestrator> agent_orchestrator;

    // Route handlers
    void handle_agent_health(SocketType sock);
    void handle_agent_metrics(SocketType sock);
    void handle_agent_status(SocketType sock, const std::string& agent_id);
    void handle_system_metrics(SocketType sock);
    void handle_orchestrator_status(SocketType sock);
    void handle_workflow_metrics(SocketType sock);

    // Utility methods
    std::string extract_agent_id_from_path(const std::string& path);
    void send_json_response(SocketType sock, int status_code, const std::string& json_content);
    void send_error_response(SocketType sock, int status_code, const std::string& error_message);
};

} // namespace kolosal

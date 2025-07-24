#pragma once

#include "../export.hpp"
#include "../server.hpp"
#include "../agents/agent_orchestrator.hpp"
#include "route_interface.hpp"
#include <memory>
#include <json.hpp>

namespace kolosal::routes {

/**
 * @brief Agent orchestration and workflow management routes
 */
class KOLOSAL_SERVER_API OrchestrationRoute : public IRoute {
private:
    std::shared_ptr<agents::AgentOrchestrator> orchestrator;
    
    // Store current request context
    mutable std::string current_method;
    mutable std::string current_path;

public:
    explicit OrchestrationRoute(std::shared_ptr<agents::AgentOrchestrator> orch);
    
    // Method to register routes with the server
    void setup_routes(Server& server);
    
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    // Helper methods for parsing requests and sending responses
    std::string parse_request_body(const std::string& body);
    void send_response(SocketType sock, int status_code, const std::string& content);
    void send_json_response(SocketType sock, int status_code, const nlohmann::json& data);
    void send_error_response(SocketType sock, int status_code, const std::string& error);
    void send_success_response(SocketType sock, const nlohmann::json& data = {});
    
    // Route handlers
    void handle_workflow_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body);
    void handle_collaboration_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body);
    void handle_coordination_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body);
    void handle_monitoring_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body);
    void handle_loadbalancing_routes(SocketType sock, const std::string& method, const std::string& path, const std::string& body);
    
    // Individual endpoint handlers
    void handle_create_workflow(SocketType sock, const std::string& body);
    void handle_list_workflows(SocketType sock);
    void handle_get_workflow(SocketType sock, const std::string& workflow_id);
    void handle_execute_workflow(SocketType sock, const std::string& workflow_id, const std::string& body);
    void handle_execute_workflow_async(SocketType sock, const std::string& workflow_id, const std::string& body);
    void handle_get_workflow_result(SocketType sock, const std::string& workflow_id);
    void handle_cancel_workflow(SocketType sock, const std::string& workflow_id);
    void handle_delete_workflow(SocketType sock, const std::string& workflow_id);
    
    void handle_create_collaboration_group(SocketType sock, const std::string& body);
    void handle_list_collaboration_groups(SocketType sock);
    void handle_execute_collaboration(SocketType sock, const std::string& group_id, const std::string& body);
    void handle_get_collaboration_result(SocketType sock, const std::string& group_id);
    void handle_delete_collaboration_group(SocketType sock, const std::string& group_id);
    
    void handle_coordinate_agents(SocketType sock, const std::string& body);
    void handle_setup_pipeline(SocketType sock, const std::string& body);
    void handle_execute_pipeline(SocketType sock, const std::string& pipeline_id, const std::string& body);
    
    void handle_get_orchestration_metrics(SocketType sock);
    void handle_get_orchestrator_status(SocketType sock);
    void handle_get_active_workflows(SocketType sock);
    
    void handle_select_optimal_agent(SocketType sock, const std::string& body);
    void handle_distribute_workload(SocketType sock, const std::string& body);
    void handle_optimize_allocation(SocketType sock, const std::string& body);
    
    // Helper methods for validation and parsing
    bool validate_workflow_definition(const nlohmann::json& workflow);
    bool validate_collaboration_group(const nlohmann::json& group);
    agents::AgentOrchestrator::CollaborationPattern parse_collaboration_pattern(const std::string& pattern);
    nlohmann::json workflow_result_to_json(const agents::AgentOrchestrator::WorkflowResult& result);
    
    // URL path parsing helpers
    std::string extract_id_from_path(const std::string& path, const std::string& pattern);
    bool path_matches(const std::string& path, const std::string& pattern);
    std::vector<std::string> split_path(const std::string& path);
};

} // namespace kolosal::routes

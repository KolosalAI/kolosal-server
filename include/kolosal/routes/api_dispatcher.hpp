#pragma once

#include "../export.hpp"
#include "../server.hpp"
#include "route_interface.hpp"
#include "agents_route.hpp"
#include "orchestration_route.hpp"
#include "../agents/multi_agent_system.hpp"
#include "../agents/workflow_engine.hpp"
#include <memory>
#include <json.hpp>
#include <regex>

namespace kolosal::routes {

/**
 * @brief Main API route dispatcher for all agent and orchestration endpoints
 */
class KOLOSAL_SERVER_API APIDispatcher : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    std::shared_ptr<agents::WorkflowEngine> workflow_engine;
    std::unique_ptr<AgentsRoute> agents_route;
    std::unique_ptr<OrchestrationRoute> orchestration_route;
    
    // Store current request context
    mutable std::string current_method;
    mutable std::string current_path;
    mutable std::string current_body;

public:
    explicit APIDispatcher(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager,
                          std::shared_ptr<agents::WorkflowEngine> engine = nullptr);
    
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    void handle_agent_endpoints(SocketType sock, const std::string& method, const std::string& path, const std::string& body);
    void handle_orchestration_endpoints(SocketType sock, const std::string& method, const std::string& path, const std::string& body);
    
    // Agent management endpoints
    void dispatch_agent_create(SocketType sock, const std::string& body);
    void dispatch_agent_list(SocketType sock);
    void dispatch_agent_get(SocketType sock, const std::string& agent_id);
    void dispatch_agent_update(SocketType sock, const std::string& agent_id, const std::string& body);
    void dispatch_agent_delete(SocketType sock, const std::string& agent_id);
    void dispatch_agent_system_status(SocketType sock);
    
    // Agent lifecycle endpoints
    void dispatch_agent_start(SocketType sock, const std::string& agent_id);
    void dispatch_agent_stop(SocketType sock, const std::string& agent_id);
    void dispatch_agent_restart(SocketType sock, const std::string& agent_id);
    void dispatch_agent_status(SocketType sock, const std::string& agent_id);
    
    // Agent function endpoints
    void dispatch_agent_capabilities(SocketType sock, const std::string& agent_id);
    void dispatch_agent_functions(SocketType sock, const std::string& agent_id);
    void dispatch_agent_function_execute(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body);
    void dispatch_agent_function_test(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body);
    
    // Agent templates endpoints
    void dispatch_agent_templates_list(SocketType sock);
    void dispatch_agent_template_create(SocketType sock, const std::string& template_name, const std::string& body);
    
    // Bulk operations endpoints
    void dispatch_agent_bulk_start(SocketType sock, const std::string& body);
    void dispatch_agent_bulk_stop(SocketType sock, const std::string& body);
    void dispatch_agent_bulk_delete(SocketType sock, const std::string& body);
    
    // Workflow management endpoints
    void dispatch_workflow_create(SocketType sock, const std::string& body);
    void dispatch_workflow_list(SocketType sock);
    void dispatch_workflow_get(SocketType sock, const std::string& workflow_id);
    void dispatch_workflow_update(SocketType sock, const std::string& workflow_id, const std::string& body);
    void dispatch_workflow_delete(SocketType sock, const std::string& workflow_id);
    void dispatch_workflow_execute(SocketType sock, const std::string& workflow_id, const std::string& body);
    void dispatch_workflow_pause(SocketType sock, const std::string& execution_id);
    void dispatch_workflow_resume(SocketType sock, const std::string& execution_id);
    void dispatch_workflow_cancel(SocketType sock, const std::string& execution_id);
    void dispatch_workflow_status(SocketType sock, const std::string& workflow_id);
    void dispatch_execution_status(SocketType sock, const std::string& execution_id);
    void dispatch_workflow_history(SocketType sock, const std::string& workflow_id = "");
    
    // Workflow template endpoints
    void dispatch_workflow_template_sequential(SocketType sock, const std::string& body);
    void dispatch_workflow_template_parallel(SocketType sock, const std::string& body);
    void dispatch_workflow_template_pipeline(SocketType sock, const std::string& body);
    void dispatch_workflow_template_consensus(SocketType sock, const std::string& body);
    
    // Monitoring and metrics endpoints
    void dispatch_orchestration_status(SocketType sock);
    void dispatch_orchestration_metrics(SocketType sock);
    void dispatch_active_workflows(SocketType sock);
    
    // Helper methods
    void send_response(SocketType sock, int status_code, const std::string& content);
    void send_json_response(SocketType sock, int status_code, const nlohmann::json& data);
    void send_error_response(SocketType sock, int status_code, const std::string& error);
    void send_success_response(SocketType sock, const nlohmann::json& data = {});
    
    std::string extract_path_parameter(const std::string& path, const std::string& pattern, int param_index);
};

} // namespace kolosal::routes

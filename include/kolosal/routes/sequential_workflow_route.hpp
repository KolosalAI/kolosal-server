#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include "../agents/sequential_workflow.hpp"
#include "../agents/multi_agent_system.hpp"
#include <memory>
#include <string>

namespace kolosal::routes {

/**
 * @brief Route handler for sequential workflow operations
 */
class KOLOSAL_SERVER_API SequentialWorkflowRoute : public IRoute {
private:
    std::shared_ptr<agents::SequentialWorkflowExecutor> workflow_executor;
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    
    // Store current request context for routing
    mutable std::string last_matched_method;
    mutable std::string last_matched_path;
    std::string current_method;
    std::string current_path;

public:
    explicit SequentialWorkflowRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager);
    ~SequentialWorkflowRoute() override = default;

    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    // Helper methods for different endpoints
    void handle_list_workflows(SocketType sock);
    void handle_get_workflow(SocketType sock, const std::string& workflow_id);
    void handle_create_workflow(SocketType sock, const std::string& body);
    void handle_execute_workflow(SocketType sock, const std::string& workflow_id, const std::string& body);
    void handle_execute_workflow_async(SocketType sock, const std::string& workflow_id, const std::string& body);
    void handle_get_workflow_result(SocketType sock, const std::string& workflow_id);
    void handle_get_workflow_status(SocketType sock, const std::string& workflow_id);
    void handle_cancel_workflow(SocketType sock, const std::string& workflow_id);
    void handle_delete_workflow(SocketType sock, const std::string& workflow_id);
    void handle_get_executor_metrics(SocketType sock);
    void handle_create_workflow_from_template(SocketType sock, const std::string& body);
    
    // Utility methods
    std::string format_success_response(const nlohmann::json& data);
    std::string format_error_response(const std::string& message, int error_code = 500);
    agents::SequentialWorkflow parse_workflow_from_json(const nlohmann::json& json_data);
    agents::SequentialWorkflowStep parse_step_from_json(const nlohmann::json& json_step);
    nlohmann::json workflow_to_json(const agents::SequentialWorkflow& workflow);
    nlohmann::json workflow_result_to_json(const agents::SequentialWorkflowResult& result);
    std::string extract_workflow_id_from_path(const std::string& path);
    bool is_valid_workflow_id(const std::string& workflow_id);
};

} // namespace kolosal::routes

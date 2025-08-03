#pragma once

#include "../route_interface.hpp"
#include "../../export.hpp"
#include "../../server.hpp"
#include "../../agents/multi_agent_system.hpp"
#include "../../agents/document_agent_service.hpp"
#include "../../agents/workflow_agent_service.hpp"
#include <memory>
#include <json.hpp>

namespace kolosal::routes::agents {

// Forward declarations
class AgentCore;

/**
 * @brief Comprehensive agent management routes including document and workflow services
 */
class KOLOSAL_SERVER_API AgentsRoute : public IRoute {
private:
    std::shared_ptr<kolosal::agents::YAMLConfigurableAgentManager> agent_manager;
    std::unique_ptr<kolosal::agents::DocumentAgentService> document_service;
    std::unique_ptr<kolosal::agents::WorkflowAgentService> workflow_service;

public:
    explicit AgentsRoute(std::shared_ptr<kolosal::agents::YAMLConfigurableAgentManager> manager);
    ~AgentsRoute() override = default;
    
    // IRoute interface implementation
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;
    
    void setup_routes(Server& server);

    // Core agent management API endpoints
    void handle_create_agent(SocketType sock, const std::string& body);
    void handle_list_agents(SocketType sock);
    void handle_get_agent(SocketType sock, const std::string& agent_id);
    void handle_update_agent(SocketType sock, const std::string& agent_id, const std::string& body);
    void handle_delete_agent(SocketType sock, const std::string& agent_id);
    void handle_agent_system_status(SocketType sock);
    void handle_agent_system_metrics(SocketType sock);
    
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
    
    // Agent messaging with model selection
    void handle_send_message_to_agent(SocketType sock, const std::string& agent_id, const std::string& body);
    
    // Agent templates and presets
    void handle_list_agent_templates(SocketType sock);
    void handle_create_agent_from_template(SocketType sock, const std::string& template_name, const std::string& body);
    
    // Bulk operations
    void handle_bulk_start_agents(SocketType sock, const std::string& body);
    void handle_bulk_stop_agents(SocketType sock, const std::string& body);
    void handle_bulk_delete_agents(SocketType sock, const std::string& body);

    // Document agent service endpoints
    void handle_bulk_documents(SocketType sock, const std::string& body);
    void handle_bulk_retrieval(SocketType sock, const std::string& body);
    void handle_document_search(SocketType sock, const std::string& body);
    void handle_document_upload(SocketType sock, const std::string& body);
    void handle_list_collections(SocketType sock);
    void handle_create_collection(SocketType sock, const std::string& body);
    void handle_delete_collection(SocketType sock, const std::string& collection_name);
    void handle_get_collection_info(SocketType sock, const std::string& collection_name);

    // Workflow agent service endpoints
    void handle_create_workflow(SocketType sock, const std::string& body);
    void handle_execute_workflow(SocketType sock, const std::string& body);
    void handle_get_workflow_status(SocketType sock, const std::string& workflow_id);
    void handle_list_workflows(SocketType sock);
    void handle_delete_workflow(SocketType sock, const std::string& workflow_id);
    void handle_rag_workflow(SocketType sock, const std::string& body);
    void handle_rag_search(SocketType sock, const std::string& body);
    
    // Session management endpoints
    void handle_create_session(SocketType sock, const std::string& body);
    void handle_get_session(SocketType sock, const std::string& session_id);
    void handle_list_sessions(SocketType sock);
    void handle_delete_session(SocketType sock, const std::string& session_id);
    void handle_session_history(SocketType sock, const std::string& session_id);
    
    // Orchestration endpoints
    void handle_create_orchestration(SocketType sock, const std::string& body);
    void handle_execute_orchestration(SocketType sock, const std::string& plan_id, const std::string& body);
    void handle_orchestration_status(SocketType sock, const std::string& plan_id);

    // Helper methods
    std::string format_error_response(const std::string& error, int code = 500);
    std::string format_success_response(const nlohmann::json& data = {});
    bool validate_agent_config(const nlohmann::json& config);
    bool validate_message_payload(const nlohmann::json& payload);
    nlohmann::json agent_to_json(const std::shared_ptr<kolosal::agents::AgentCore>& agent);
    nlohmann::json create_agent_metrics(const std::shared_ptr<kolosal::agents::AgentCore>& agent);
    
private:
    void send_response(SocketType sock, int status_code, const std::string& content);
    void send_json_response(SocketType sock, int status_code, const nlohmann::json& data);
    void send_error_response(SocketType sock, int status_code, const std::string& error);
    void send_success_response(SocketType sock, const nlohmann::json& data = {});
    
    // Route parsing helpers
    std::string current_method;
    std::string current_path;
    std::string extractIdFromPath(const std::string& path, const std::string& base_pattern);
    bool matchesPattern(const std::string& path, const std::string& pattern);
};

} // namespace kolosal::routes::agents

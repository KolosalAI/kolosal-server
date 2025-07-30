#pragma once

#include "../route_interface.hpp"
#include "../../export.hpp"
#include "../../server.hpp"
#include <memory>
#include <json.hpp>

namespace kolosal::routes::agents {

/**
 * @brief Sequential workflow route handler
 * 
 * This route implements the sequential workflow endpoints:
 * - POST /api/v1/sequential/workflows - Create a new sequential workflow
 * - GET /api/v1/sequential/workflows - List all sequential workflows
 * - GET /api/v1/sequential/workflows/{id} - Get workflow details
 * - POST /api/v1/sequential/workflows/{id}/execute - Execute workflow
 * - GET /api/v1/sequential/workflows/{id}/status - Get workflow status
 * - DELETE /api/v1/sequential/workflows/{id} - Delete workflow
 */
class KOLOSAL_SERVER_API SequentialRoute : public IRoute {
public:
    SequentialRoute();
    ~SequentialRoute() override = default;
    
    // IRoute interface implementation
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    // Sequential workflow management endpoints
    void handle_create_workflow(SocketType sock, const std::string& body);
    void handle_list_workflows(SocketType sock);
    void handle_get_workflow(SocketType sock, const std::string& workflow_id);
    void handle_execute_workflow(SocketType sock, const std::string& workflow_id, const std::string& body);
    void handle_get_workflow_status(SocketType sock, const std::string& workflow_id);
    void handle_delete_workflow(SocketType sock, const std::string& workflow_id);
    
    // Helper methods
    void send_response(SocketType sock, int status_code, const std::string& content);
    void send_json_response(SocketType sock, int status_code, const nlohmann::json& data);
    void send_error_response(SocketType sock, int status_code, const std::string& error);
    void send_success_response(SocketType sock, const nlohmann::json& data = {});
    
    // Route parsing helpers
    std::string current_method_;
    std::string current_path_;
    std::string extractIdFromPath(const std::string& path, const std::string& base_pattern);
    bool validate_workflow_config(const nlohmann::json& config);
};

} // namespace kolosal::routes::agents

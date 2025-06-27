#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include <json.hpp>
#include <memory>
#include <string>

namespace kolosal {
    class AutoSetupManager;
}

namespace kolosal::routes {

/**
 * @brief Route handler for auto-setup operations and status
 * 
 * This route provides endpoints for:
 * - Getting auto-setup status
 * - Triggering manual auto-setup
 * - Getting agent name mappings
 * - Getting available engines status
 */
class KOLOSAL_SERVER_API AutoSetupRoute : public IRoute {
private:
    // Store current request context for routing
    mutable std::string last_matched_method;
    mutable std::string last_matched_path;
    std::string current_method;
    std::string current_path;

public:
    AutoSetupRoute();
    ~AutoSetupRoute() override = default;

    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    // Helper methods for different endpoints
    void handle_get_status(SocketType sock);
    void handle_trigger_auto_setup(SocketType sock);
    void handle_get_agent_mappings(SocketType sock);
    void handle_get_engine_status(SocketType sock);
    void handle_validate_workflow(SocketType sock, const std::string& body);
    
    // Utility methods
    std::string format_success_response(const nlohmann::json& data);
    std::string format_error_response(const std::string& message, int error_code = 500);
    AutoSetupManager& get_auto_setup_manager();
};

} // namespace kolosal::routes

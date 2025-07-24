#pragma once

#include "../export.hpp"
#include "../server.hpp"
#include "../agents/multi_agent_system.hpp"
#include <memory>
#include <json.hpp>

namespace kolosal::routes {

/**
 * @brief Agent management routes
 */
class KOLOSAL_SERVER_API AgentsRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;

public:
    explicit AgentsRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager);
    
    void setup_routes(Server& server);

    // Helper methods
    std::string format_error_response(const std::string& error, int code = 500);
    std::string format_success_response(const nlohmann::json& data = {});
    bool validate_agent_config(const nlohmann::json& config);
    bool validate_message_payload(const nlohmann::json& payload);
};

} // namespace kolosal::routes

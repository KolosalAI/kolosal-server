#pragma once

#include "../export.hpp"
#include "../server.hpp"
#include "../agents/multi_agent_system.hpp"
#include <memory>
#include <json.hpp>

namespace kolosal::routes {

/**
 * @brief Agent document management routes
 */
class KOLOSAL_SERVER_API AgentDocumentsRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;

public:
    explicit AgentDocumentsRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager);
    
    void setup_routes(Server& server);

    // Helper methods
    std::string format_error_response(const std::string& error, int code = 500);
    std::string format_success_response(const nlohmann::json& data = {});
};

} // namespace kolosal::routes

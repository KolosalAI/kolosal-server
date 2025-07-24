#pragma once

#include "route_interface.hpp"
#include "../agents/multi_agent_system.hpp"
#include "../export.hpp"

#include <memory>
#include <string>
#include <json.hpp>

namespace kolosal::routes {

/**
 * @brief Route handler for agent collaboration endpoints
 * 
 * Implements the /api/v1/collaboration endpoints for agent collaboration and communication
 */
class KOLOSAL_SERVER_API CollaborationRoute : public IRoute {
public:
    /**
     * @brief Constructor
     */
    CollaborationRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager);

    /**
     * @brief Destructor
     */
    ~CollaborationRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the collaboration request
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handle(SocketType sock, const std::string& body) override;

private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    
    // Store current request context
    mutable std::string current_method;
    mutable std::string current_path;

    /**
     * @brief Sends error response to client
     */
    void sendErrorResponse(SocketType sock, int status, const std::string& message, 
                          const std::string& error_type = "invalid_request_error");

    /**
     * @brief Sends success response to client
     */
    void sendSuccessResponse(SocketType sock, const nlohmann::json& data);

    /**
     * @brief Handle collaboration group management
     */
    void handleCollaborationGroups(SocketType sock, const std::string& body);

    /**
     * @brief Handle direct agent communication
     */
    void handleDirectCommunication(SocketType sock, const std::string& body);

    /**
     * @brief Handle broadcast communication
     */
    void handleBroadcastCommunication(SocketType sock, const std::string& body);

    /**
     * @brief Extract group ID from path
     */
    std::string extractGroupId(const std::string& path);
};

} // namespace kolosal::routes

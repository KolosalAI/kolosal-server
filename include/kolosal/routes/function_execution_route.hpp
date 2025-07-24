#pragma once

#include "route_interface.hpp"
#include "../agents/multi_agent_system.hpp"
#include "../export.hpp"

#include <memory>
#include <string>
#include <json.hpp>

namespace kolosal::routes {

/**
 * @brief Route handler for function execution endpoints
 * 
 * Implements the /api/v1/functions endpoints for executing agent functions
 */
class KOLOSAL_SERVER_API FunctionExecutionRoute : public IRoute {
public:
    /**
     * @brief Constructor
     */
    FunctionExecutionRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager);

    /**
     * @brief Destructor
     */
    ~FunctionExecutionRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the function execution request
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
     * @param sock Socket for the connection
     * @param status HTTP status code
     * @param message Error message
     * @param error_type Error type
     * @param param Parameter that caused the error (optional)
     */
    void sendErrorResponse(SocketType sock, int status, const std::string& message, 
                          const std::string& error_type = "invalid_request_error", 
                          const std::string& param = "");

    /**
     * @brief Sends success response to client
     * @param sock Socket for the connection
     * @param data Response data
     */
    void sendSuccessResponse(SocketType sock, const nlohmann::json& data);

    /**
     * @brief Handle function execution requests
     * @param sock Socket for the connection
     * @param body Request body
     * @param is_async Whether this is an async execution request
     */
    void handleFunctionExecution(SocketType sock, const std::string& body, bool is_async = false);

    /**
     * @brief Extract agent ID from path
     * @param path Request path
     * @return Agent ID or empty string if not found
     */
    std::string extractAgentId(const std::string& path);
    
    /**
     * @brief Extract function name from path
     * @param path Request path
     * @return Function name or empty string if not found
     */
    std::string extractFunctionName(const std::string& path);
};

} // namespace kolosal::routes

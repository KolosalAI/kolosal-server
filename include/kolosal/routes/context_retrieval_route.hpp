#pragma once

#include "route_interface.hpp"
#include "../export.hpp"
#include "../retrieval/document_service.hpp"
#include <string>
#include <memory>
#include <json.hpp>

namespace kolosal::routes
{

/**
 * @brief Route handler for context retrieval endpoint
 * 
 * Implements the /context-retrieval endpoint for semantic search and context retrieval
 */
class KOLOSAL_SERVER_API ContextRetrievalRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    ContextRetrievalRoute();

    /**
     * @brief Destructor
     */
    ~ContextRetrievalRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the context retrieval request
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handle(SocketType sock, const std::string& body) override;

private:
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
     * @brief Handle POST requests for context retrieval
     * @param sock Socket for the connection
     * @param body Request body
     */
    void handleContextRetrieval(SocketType sock, const std::string& body);

    /**
     * @brief Handle GET requests for context retrieval info
     * @param sock Socket for the connection
     */
    void handleContextRetrievalInfo(SocketType sock);
};

} // namespace kolosal::routes

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
 * @brief Route handler for vector search endpoint
 * 
 * Implements the /vector-search endpoint for direct vector similarity search
 */
class KOLOSAL_SERVER_API VectorSearchRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    VectorSearchRoute();

    /**
     * @brief Destructor
     */
    ~VectorSearchRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the vector search request
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
     * @brief Handle POST requests for vector search
     * @param sock Socket for the connection
     * @param body Request body
     */
    void handleVectorSearch(SocketType sock, const std::string& body);

    /**
     * @brief Handle GET requests for vector search info
     * @param sock Socket for the connection
     */
    void handleVectorSearchInfo(SocketType sock);
};

} // namespace kolosal::routes

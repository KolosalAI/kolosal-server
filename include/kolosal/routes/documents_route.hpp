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
 * @brief Route handler for document management endpoints
 * 
 * Implements the /api/v1/documents endpoints for CRUD operations on documents
 */
class KOLOSAL_SERVER_API DocumentsRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    DocumentsRoute();

    /**
     * @brief Destructor
     */
    ~DocumentsRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the document management request
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
     * @brief Handle GET requests for listing documents
     * @param sock Socket for the connection
     * @param path Request path
     */
    void handleGetDocuments(SocketType sock, const std::string& path);

    /**
     * @brief Handle POST requests for creating documents
     * @param sock Socket for the connection
     * @param body Request body
     */
    void handlePostDocuments(SocketType sock, const std::string& body);

    /**
     * @brief Handle PUT requests for updating documents
     * @param sock Socket for the connection
     * @param path Request path
     * @param body Request body
     */
    void handlePutDocuments(SocketType sock, const std::string& path, const std::string& body);

    /**
     * @brief Handle DELETE requests for removing documents
     * @param sock Socket for the connection
     * @param path Request path
     */
    void handleDeleteDocuments(SocketType sock, const std::string& path);

    /**
     * @brief Extract document ID from path
     * @param path Request path
     * @return Document ID or empty string if not found
     */
    std::string extractDocumentId(const std::string& path);
};

} // namespace kolosal::routes

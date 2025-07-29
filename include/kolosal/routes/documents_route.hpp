#pragma once

#include "route_interface.hpp"
#include "../export.hpp"
#include "../retrieval/document_service.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace kolosal
{

/**
 * @brief Combined route handler for document operations
 * 
 * This route implements multiple document endpoints:
 * - POST /add_documents - Add documents to vector database
 * - POST /remove_documents - Remove documents by IDs
 * - GET /list_documents - List all document IDs
 * - POST /documents_info - Get full document information by IDs
 * 
 * All implementations are fully async and thread-safe.
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
     * @brief Handles the document request based on the endpoint
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handle(SocketType sock, const std::string& body) override;

private:
    /**
     * @brief Handles add documents request
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handleAddDocuments(SocketType sock, const std::string& body);

    /**
     * @brief Handles remove documents request
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handleRemoveDocuments(SocketType sock, const std::string& body);

    /**
     * @brief Handles list documents request
     * @param sock Socket for the connection
     */
    void handleListDocuments(SocketType sock);

    /**
     * @brief Handles documents info request
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handleDocumentsInfo(SocketType sock, const std::string& body);

    /**
     * @brief Handles OPTIONS requests for CORS preflight
     * @param sock Socket for the connection
     */
    void handleOptions(SocketType sock);

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
     * @brief Initializes the document service if needed
     * @return true if service is ready, false otherwise
     */
    bool ensureDocumentService();

    static std::atomic<long long> request_counter_;
    std::unique_ptr<kolosal::retrieval::DocumentService> document_service_;
    std::mutex service_mutex_;
    std::string current_endpoint_;
    std::string current_method_;
};

} // namespace kolosal

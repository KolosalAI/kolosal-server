#pragma once

#include "../routes/route_interface.hpp"
#include "../export.hpp"
#include "document_service.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace kolosal
{

class CompletionMonitor;

namespace retrieval
{

/**
 * @brief Route handler for adding documents to vector database
 * 
 * This route implements the /add_documents endpoint for indexing documents
 * into the vector database with automatic embedding generation.
 * The implementation is fully async and thread-safe.
 */
class KOLOSAL_SERVER_API AddDocumentsRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    AddDocumentsRoute();

    /**
     * @brief Destructor
     */
    ~AddDocumentsRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the add documents request
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

    static std::atomic<long long> request_counter_;
    std::unique_ptr<CompletionMonitor> monitor_;
    std::unique_ptr<DocumentService> document_service_;
    std::mutex service_mutex_;
};

} // namespace retrieval
} // namespace kolosal

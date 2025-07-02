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

class CompletionMonitor;

/**
 * @brief Route handler for document retrieval (vector search)
 * 
 * This route implements the /retrieve endpoint for performing vector similarity search
 * on the document database. It embeds the query and returns the most similar documents.
 * The implementation is fully async and thread-safe.
 */
class KOLOSAL_SERVER_API RetrieveRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    RetrieveRoute();

    /**
     * @brief Destructor
     */
    ~RetrieveRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the retrieve request
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
    std::unique_ptr<kolosal::retrieval::DocumentService> document_service_;
    std::mutex service_mutex_;
};

} // namespace kolosal

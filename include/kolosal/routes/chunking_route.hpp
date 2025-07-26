#ifndef KOLOSAL_CHUNKING_ROUTE_HPP
#define KOLOSAL_CHUNKING_ROUTE_HPP

#include "route_interface.hpp"
#include "../export.hpp"
#include "../retrieval/chunking_types.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace kolosal
{

/**
 * @brief Route handler for text chunking requests
 * 
 * This route implements the /chunking endpoint for text chunking operations.
 * It supports both regular and semantic chunking methods.
 * The implementation is fully async and thread-safe.
 */
class KOLOSAL_SERVER_API ChunkingRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    ChunkingRoute();

    /**
     * @brief Destructor
     */
    ~ChunkingRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the chunking request
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handle(SocketType sock, const std::string& body) override;

private:
    /**
     * @brief Processes regular chunking request
     * @param request Chunking request parameters
     * @return Future containing chunked text
     */
    std::future<std::vector<std::string>> processRegularChunking(
        const std::string& text,
        const std::string& model_name,
        int chunk_size,
        int overlap
    );

    /**
     * @brief Processes semantic chunking request
     * @param request Chunking request parameters
     * @return Future containing semantically chunked text
     */
    std::future<std::vector<std::string>> processSemanticChunking(
        const std::string& text,
        const std::string& model_name,
        int chunk_size,
        int overlap,
        int max_chunk_size,
        float similarity_threshold
    );

    /**
     * @brief Sends an error response
     * @param sock Socket for the connection
     * @param status_code HTTP status code
     * @param error_message Error message
     * @param error_type Error type (optional)
     * @param param Parameter name causing error (optional)
     */
    void sendErrorResponse(
        SocketType sock,
        int status_code,
        const std::string& error_message,
        const std::string& error_type = "processing_error",
        const std::string& param = ""
    );

    /**
     * @brief Estimates token count for text
     * @param text Input text
     * @return Estimated token count
     */
    int estimateTokenCount(const std::string& text) const;

    /**
     * @brief Validates that the model can be used for chunking
     * @param model_name Name of the model to validate
     * @return true if valid, false otherwise
     */
    bool validateChunkingModel(const std::string& model_name) const;

    // Private members
    std::unique_ptr<retrieval::ChunkingService> chunking_service_;
    std::atomic<uint64_t> request_counter_;
    std::mutex service_mutex_;
};

} // namespace kolosal

#endif // KOLOSAL_CHUNKING_ROUTE_HPP

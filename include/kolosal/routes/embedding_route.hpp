#ifndef KOLOSAL_EMBEDDING_ROUTE_HPP
#define KOLOSAL_EMBEDDING_ROUTE_HPP

#include "route_interface.hpp"
#include "../export.hpp"
#include <string>
#include <memory>
#include <future>
#include <atomic>
#include <mutex>

namespace kolosal
{

class CompletionMonitor;

/**
 * @brief Route handler for embedding requests
 * 
 * This route implements the /v1/embeddings endpoint following the OpenAI embeddings API.
 * It supports both single text and batch text embedding requests.
 * The implementation is fully async and thread-safe.
 */
class KOLOSAL_SERVER_API EmbeddingRoute : public IRoute
{
public:
    /**
     * @brief Constructor
     */
    EmbeddingRoute();

    /**
     * @brief Destructor
     */
    ~EmbeddingRoute();

    /**
     * @brief Checks if this route matches the request
     * @param method HTTP method
     * @param path Request path
     * @return true if route matches, false otherwise
     */
    bool match(const std::string& method, const std::string& path) override;

    /**
     * @brief Handles the embedding request
     * @param sock Socket for the connection
     * @param body Request body JSON
     */
    void handle(SocketType sock, const std::string& body) override;

private:
    /**
     * @brief Processes a single embedding request
     * @param input_text Text to embed
     * @param model Model name
     * @param request_id Request ID for monitoring
     * @return Future containing the embedding vector
     */
    std::future<std::vector<float>> processEmbeddingAsync(
        const std::string& input_text, 
        const std::string& model,
        const std::string& request_id
    );

    /**
     * @brief Processes multiple embedding requests in parallel
     * @param input_texts Vector of texts to embed
     * @param model Model name
     * @param request_id Base request ID for monitoring
     * @return Vector of futures containing embedding vectors
     */
    std::vector<std::future<std::vector<float>>> processEmbeddingsBatch(
        const std::vector<std::string>& input_texts,
        const std::string& model,
        const std::string& request_id
    );

    /**
     * @brief Estimates the number of tokens in a text string
     * @param text Input text
     * @return Estimated token count
     */
    int estimateTokenCount(const std::string& text) const;

    /**
     * @brief Validates that the model supports embeddings
     * @param model Model name
     * @return true if model supports embeddings, false otherwise
     */
    bool validateEmbeddingModel(const std::string& model) const;

    /**
     * @brief Sends an error response
     * @param sock Socket connection
     * @param status_code HTTP status code
     * @param error_message Error message
     * @param error_type Error type (default: "invalid_request_error")
     * @param param Parameter that caused the error (default: empty)
     */
    void sendErrorResponse(
        SocketType sock, 
        int status_code, 
        const std::string& error_message,
        const std::string& error_type = "invalid_request_error",
        const std::string& param = ""
    );

    // Thread-safe monitoring
    CompletionMonitor* monitor_;
    
    // Request counter for unique IDs
    std::atomic<uint64_t> request_counter_;
    
    // Mutex for thread safety
    mutable std::mutex route_mutex_;
};

} // namespace kolosal

#endif // KOLOSAL_EMBEDDING_ROUTE_HPP

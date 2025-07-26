#ifndef KOLOSAL_CHUNKING_TYPES_HPP
#define KOLOSAL_CHUNKING_TYPES_HPP

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <mutex>

namespace kolosal
{

// Forward declarations
class InferenceEngine;

namespace retrieval
{

/**
 * @brief Service for text chunking operations
 * 
 * This service provides both regular and semantic chunking capabilities
 * for text documents. It supports concurrent processing and is thread-safe.
 */
class ChunkingService
{
public:
    /**
     * @brief Constructor
     */
    ChunkingService();
    
    /**
     * @brief Destructor
     */
    ~ChunkingService();

    /**
     * @brief Generates base chunks from text using sliding window approach
     * 
     * @param text Input text to chunk
     * @param tokens Vector of tokens from the text
     * @param chunk_size Number of tokens per chunk
     * @param overlap Number of tokens to overlap between chunks
     * @return Vector of text chunks
     */
    std::vector<std::string> generateBaseChunks(
        const std::string& text,
        const std::vector<std::string>& tokens,
        int chunk_size,
        int overlap
    ) const;

    /**
     * @brief Performs semantic chunking using embeddings
     * 
     * @param text Input text to chunk
     * @param model_name Name of the embedding model to use
     * @param chunk_size Base chunk size in tokens
     * @param overlap Overlap between base chunks
     * @param max_tokens Maximum tokens when merging chunks
     * @param similarity_threshold Threshold for semantic similarity
     * @return Future containing vector of semantically merged chunks
     */
    std::future<std::vector<std::string>> semanticChunk(
        const std::string& text,
        const std::string& model_name,
        int chunk_size,
        int overlap,
        int max_tokens,
        float similarity_threshold
    );

    /**
     * @brief Tokenizes text using the specified model
     * 
     * @param text Input text to tokenize
     * @param model_name Name of the model to use for tokenization
     * @return Future containing vector of tokens
     */
    std::future<std::vector<std::string>> tokenizeText(
        const std::string& text,
        const std::string& model_name
    );

    /**
     * @brief Computes embedding for a text chunk
     * 
     * @param text Text to embed
     * @param model_name Name of the embedding model
     * @return Future containing embedding vector
     */
    std::future<std::vector<float>> computeEmbedding(
        const std::string& text,
        const std::string& model_name
    );

    /**
     * @brief Computes cosine similarity between two embedding vectors
     * 
     * @param a First embedding vector
     * @param b Second embedding vector
     * @return Cosine similarity value
     */
    static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

    /**
     * @brief Estimates token count for a text string
     * 
     * @param text Input text
     * @return Estimated token count
     */
    static int estimateTokenCount(const std::string& text);

    /**
     * @brief Reconstructs text from tokens
     * 
     * @param tokens Vector of tokens
     * @return Reconstructed text
     */
    static std::string reconstructText(const std::vector<std::string>& tokens);

private:
    // Internal helper methods
    void validateChunkingParameters(int chunk_size, int overlap, int max_tokens, float similarity_threshold) const;
    std::vector<std::string> extractTokenSubset(const std::vector<std::string>& tokens, int start, int end) const;
    
    // Mutex for serializing embedding requests
    mutable std::mutex embedding_mutex_;
};

} // namespace retrieval
} // namespace kolosal

#endif // KOLOSAL_CHUNKING_TYPES_HPP

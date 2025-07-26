#ifndef KOLOSAL_CHUNKING_RESPONSE_MODEL_HPP
#define KOLOSAL_CHUNKING_RESPONSE_MODEL_HPP

#include "model_interface.hpp"
#include <json.hpp>
#include <string>
#include <vector>

namespace kolosal
{

/**
 * @brief Model for a single chunk in the response
 */
class ChunkData
{
public:
    // Text content of the chunk
    std::string text;
    
    // Index of the chunk in the original sequence
    int index;
    
    // Number of tokens in this chunk
    int token_count;

    /**
     * @brief Default constructor
     */
    ChunkData() : index(0), token_count(0) {}

    /**
     * @brief Constructor with parameters
     */
    ChunkData(const std::string& t, int idx, int tokens) 
        : text(t), index(idx), token_count(tokens) {}

    /**
     * @brief Converts the chunk to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Model for chunking response
 * 
 * This model represents the response body for the /chunking endpoint
 */
class ChunkingResponse : public IModel
{
public:
    // Model name used for processing
    std::string model_name;
    
    // Method used for chunking
    std::string method;
    
    // Number of chunks generated
    int total_chunks;
    
    // Array of chunk data
    std::vector<ChunkData> chunks;
    
    // Processing statistics
    struct Usage {
        int original_tokens = 0;
        int total_chunk_tokens = 0;
        float processing_time_ms = 0.0f;
    } usage;

    /**
     * @brief Default constructor
     */
    ChunkingResponse() : total_chunks(0) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~ChunkingResponse() = default;

    /**
     * @brief Validates the chunking response
     * @return true if valid, false otherwise
     */
    bool validate() const override;

    /**
     * @brief Converts the response to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const override;

    /**
     * @brief Populates the response from JSON
     * @param j JSON object to parse
     */
    void from_json(const nlohmann::json& j) override;

    /**
     * @brief Adds a chunk to the response
     * @param chunk The chunk data to add
     */
    void addChunk(const ChunkData& chunk);

    /**
     * @brief Sets the usage statistics
     * @param original_tokens Number of tokens in original text
     * @param total_chunk_tokens Total tokens across all chunks
     * @param processing_time_ms Processing time in milliseconds
     */
    void setUsage(int original_tokens, int total_chunk_tokens, float processing_time_ms);
};

} // namespace kolosal

#endif // KOLOSAL_CHUNKING_RESPONSE_MODEL_HPP

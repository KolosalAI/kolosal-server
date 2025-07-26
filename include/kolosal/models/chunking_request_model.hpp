#ifndef KOLOSAL_CHUNKING_REQUEST_MODEL_HPP
#define KOLOSAL_CHUNKING_REQUEST_MODEL_HPP

#include "model_interface.hpp"
#include <json.hpp>
#include <string>

namespace kolosal
{

/**
 * @brief Model for chunking request
 * 
 * This model represents the request body for the /chunking endpoint
 */
class ChunkingRequest : public IModel
{
public:
    // Model name to use for embeddings (required)
    std::string model_name;
    
    // Text to be chunked (required)
    std::string text;
    
    // Base chunk size in tokens (optional, default 128)
    int chunk_size = 128;
    
    // Maximum chunk size when merging (optional, default 512)
    int max_chunk_size = 512;
    
    // Overlap between chunks in tokens (optional, default 64)
    int overlap = 64;
    
    // Similarity threshold for semantic chunking (optional, default 0.7)
    float similarity_threshold = 0.7f;
    
    // Chunking method: "semantic" or "regular" (optional, default "regular")
    std::string method = "regular";

    /**
     * @brief Default constructor
     */
    ChunkingRequest() = default;

    /**
     * @brief Virtual destructor
     */
    virtual ~ChunkingRequest() = default;

    /**
     * @brief Validates the chunking request
     * @return true if valid, false otherwise
     */
    bool validate() const override;

    /**
     * @brief Converts the request to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const override;

    /**
     * @brief Populates the request from JSON
     * @param j JSON object to parse
     */
    void from_json(const nlohmann::json& j) override;
};

} // namespace kolosal

#endif // KOLOSAL_CHUNKING_REQUEST_MODEL_HPP

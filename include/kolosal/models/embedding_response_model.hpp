#ifndef KOLOSAL_EMBEDDING_RESPONSE_MODEL_HPP
#define KOLOSAL_EMBEDDING_RESPONSE_MODEL_HPP

#include "model_interface.hpp"
#include <json.hpp>
#include <string>
#include <vector>

namespace kolosal
{

/**
 * @brief Single embedding data object
 */
struct EmbeddingData
{
    std::string object = "embedding";
    std::vector<float> embedding;
    int index = 0;

    /**
     * @brief Converts embedding data to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;

    /**
     * @brief Populates embedding data from JSON
     * @param j JSON object to parse
     */
    void from_json(const nlohmann::json& j);
};

/**
 * @brief Usage information for the embedding request
 */
struct EmbeddingUsage
{
    int prompt_tokens = 0;
    int total_tokens = 0;

    /**
     * @brief Converts usage to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;

    /**
     * @brief Populates usage from JSON
     * @param j JSON object to parse
     */
    void from_json(const nlohmann::json& j);
};

/**
 * @brief Model for embedding response
 * 
 * This model represents the response for the /v1/embeddings endpoint
 * following the OpenAI embeddings API specification.
 */
class EmbeddingResponse : public IModel
{
public:
    std::string object = "list";
    std::vector<EmbeddingData> data;
    std::string model;
    EmbeddingUsage usage;

    /**
     * @brief Default constructor
     */
    EmbeddingResponse() = default;

    /**
     * @brief Virtual destructor
     */
    virtual ~EmbeddingResponse() = default;

    /**
     * @brief Validates the embedding response
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
     * @brief Add an embedding result to the response
     * @param embedding Vector of floats representing the embedding
     * @param index Index of this embedding in the batch
     */
    void addEmbedding(const std::vector<float>& embedding, int index);

    /**
     * @brief Sets the usage statistics
     * @param prompt_tokens Number of tokens in the input
     */
    void setUsage(int prompt_tokens);
};

/**
 * @brief Error response for embedding requests
 */
class EmbeddingErrorResponse : public IModel
{
public:
    struct ErrorDetails
    {
        std::string message;
        std::string type;
        std::string param;
        std::string code;

        nlohmann::json to_json() const;
        void from_json(const nlohmann::json& j);
    };

    ErrorDetails error;

    /**
     * @brief Default constructor
     */
    EmbeddingErrorResponse() = default;

    /**
     * @brief Virtual destructor
     */
    virtual ~EmbeddingErrorResponse() = default;

    /**
     * @brief Validates the error response
     * @return true if valid, false otherwise
     */
    bool validate() const override;

    /**
     * @brief Converts the error response to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const override;

    /**
     * @brief Populates the error response from JSON
     * @param j JSON object to parse
     */
    void from_json(const nlohmann::json& j) override;
};

} // namespace kolosal

#endif // KOLOSAL_EMBEDDING_RESPONSE_MODEL_HPP

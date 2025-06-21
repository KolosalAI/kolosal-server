#ifndef KOLOSAL_EMBEDDING_REQUEST_MODEL_HPP
#define KOLOSAL_EMBEDDING_REQUEST_MODEL_HPP

#include "model_interface.hpp"
#include <json.hpp>
#include <string>
#include <variant>
#include <vector>

namespace kolosal
{

/**
 * @brief Model for embedding request
 * 
 * This model represents the request body for the /v1/embeddings endpoint
 * following the OpenAI embeddings API specification.
 */
class EmbeddingRequest : public IModel
{
public:
    // Input can be a single string or array of strings
    std::variant<std::string, std::vector<std::string>> input;
    
    // Model identifier (required)
    std::string model;
    
    // Encoding format (optional, defaults to "float")
    std::string encoding_format = "float";
    
    // Number of dimensions to return (optional)
    int dimensions = -1; // -1 means use model default
    
    // User identifier for tracking (optional)
    std::string user;

    /**
     * @brief Default constructor
     */
    EmbeddingRequest() = default;

    /**
     * @brief Virtual destructor
     */
    virtual ~EmbeddingRequest() = default;

    /**
     * @brief Validates the embedding request
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

    /**
     * @brief Gets the input as a vector of strings
     * @return Vector of input strings
     */
    std::vector<std::string> getInputTexts() const;

    /**
     * @brief Checks if the request has multiple inputs
     * @return true if multiple inputs, false if single input
     */
    bool hasMultipleInputs() const;
};

} // namespace kolosal

#endif // KOLOSAL_EMBEDDING_REQUEST_MODEL_HPP

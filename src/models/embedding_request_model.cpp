#include "kolosal/models/embedding_request_model.hpp"
#include <stdexcept>

namespace kolosal
{

bool EmbeddingRequest::validate() const
{
    // Model is required
    if (model.empty())
    {
        return false;
    }

    // Input is required and must not be empty
    if (std::holds_alternative<std::string>(input))
    {
        const std::string& str_input = std::get<std::string>(input);
        if (str_input.empty())
        {
            return false;
        }
    }
    else if (std::holds_alternative<std::vector<std::string>>(input))
    {
        const std::vector<std::string>& vec_input = std::get<std::vector<std::string>>(input);
        if (vec_input.empty())
        {
            return false;
        }
        
        // Check that all strings in the vector are non-empty
        for (const auto& str : vec_input)
        {
            if (str.empty())
            {
                return false;
            }
        }
    }
    else
    {
        return false; // Invalid input type
    }

    // Encoding format must be valid if provided
    if (!encoding_format.empty() && encoding_format != "float" && encoding_format != "base64")
    {
        return false;
    }

    // Dimensions must be positive if specified
    if (dimensions != -1 && dimensions <= 0)
    {
        return false;
    }

    return true;
}

nlohmann::json EmbeddingRequest::to_json() const
{
    nlohmann::json j;
    
    j["model"] = model;
    
    // Handle input variant
    if (std::holds_alternative<std::string>(input))
    {
        j["input"] = std::get<std::string>(input);
    }
    else if (std::holds_alternative<std::vector<std::string>>(input))
    {
        j["input"] = std::get<std::vector<std::string>>(input);
    }
    
    if (!encoding_format.empty())
    {
        j["encoding_format"] = encoding_format;
    }
    
    if (dimensions != -1)
    {
        j["dimensions"] = dimensions;
    }
    
    if (!user.empty())
    {
        j["user"] = user;
    }
    
    return j;
}

void EmbeddingRequest::from_json(const nlohmann::json& j)
{
    if (!j.contains("model"))
    {
        throw std::runtime_error("Missing required field: model");
    }
    model = j["model"];

    if (!j.contains("input"))
    {
        throw std::runtime_error("Missing required field: input");
    }

    // Handle input as either string or array of strings
    if (j["input"].is_string())
    {
        input = j["input"].get<std::string>();
    }
    else if (j["input"].is_array())
    {
        input = j["input"].get<std::vector<std::string>>();
    }
    else
    {
        throw std::runtime_error("Invalid input type: must be string or array of strings");
    }

    if (j.contains("encoding_format"))
    {
        encoding_format = j["encoding_format"];
    }

    if (j.contains("dimensions"))
    {
        dimensions = j["dimensions"];
    }

    if (j.contains("user"))
    {
        user = j["user"];
    }
}

std::vector<std::string> EmbeddingRequest::getInputTexts() const
{
    if (std::holds_alternative<std::string>(input))
    {
        return { std::get<std::string>(input) };
    }
    else if (std::holds_alternative<std::vector<std::string>>(input))
    {
        return std::get<std::vector<std::string>>(input);
    }
    return {};
}

bool EmbeddingRequest::hasMultipleInputs() const
{
    return std::holds_alternative<std::vector<std::string>>(input);
}

} // namespace kolosal

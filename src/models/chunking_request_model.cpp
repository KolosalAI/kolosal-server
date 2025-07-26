#include "kolosal/models/chunking_request_model.hpp"
#include "kolosal/logger.hpp"
#include <stdexcept>

namespace kolosal
{

bool ChunkingRequest::validate() const
{
    if (model_name.empty())
    {
        ServerLogger::logDebug("Validation failed: model_name is empty");
        return false;
    }
    
    if (text.empty())
    {
        ServerLogger::logDebug("Validation failed: text is empty");
        return false;
    }
    
    if (chunk_size <= 0 || chunk_size > 2048)
    {
        ServerLogger::logDebug("Validation failed: chunk_size must be between 1 and 2048, got %d", chunk_size);
        return false;
    }
    
    if (max_chunk_size <= 0 || max_chunk_size > 4096)
    {
        ServerLogger::logDebug("Validation failed: max_chunk_size must be between 1 and 4096, got %d", max_chunk_size);
        return false;
    }
    
    if (overlap < 0 || overlap >= chunk_size)
    {
        ServerLogger::logDebug("Validation failed: overlap must be >= 0 and < chunk_size, got %d", overlap);
        return false;
    }
    
    if (similarity_threshold < 0.0f || similarity_threshold > 1.0f)
    {
        ServerLogger::logDebug("Validation failed: similarity_threshold must be between 0.0 and 1.0, got %f", similarity_threshold);
        return false;
    }
    
    if (method != "regular" && method != "semantic")
    {
        ServerLogger::logDebug("Validation failed: method must be 'regular' or 'semantic', got '%s'", method.c_str());
        return false;
    }
    
    return true;
}

nlohmann::json ChunkingRequest::to_json() const
{
    nlohmann::json j;
    j["model_name"] = model_name;
    j["text"] = text;
    j["chunk_size"] = chunk_size;
    j["max_chunk_size"] = max_chunk_size;
    j["overlap"] = overlap;
    j["similarity_threshold"] = similarity_threshold;
    j["method"] = method;
    return j;
}

void ChunkingRequest::from_json(const nlohmann::json& j)
{
    if (!j.contains("model_name") || !j["model_name"].is_string())
    {
        throw std::runtime_error("Missing or invalid 'model_name' field - must be a string");
    }
    model_name = j["model_name"].get<std::string>();
    
    if (!j.contains("text") || !j["text"].is_string())
    {
        throw std::runtime_error("Missing or invalid 'text' field - must be a string");
    }
    text = j["text"].get<std::string>();
    
    if (j.contains("chunk_size"))
    {
        if (!j["chunk_size"].is_number_integer())
        {
            throw std::runtime_error("Field 'chunk_size' must be an integer");
        }
        chunk_size = j["chunk_size"].get<int>();
    }
    
    if (j.contains("max_chunk_size"))
    {
        if (!j["max_chunk_size"].is_number_integer())
        {
            throw std::runtime_error("Field 'max_chunk_size' must be an integer");
        }
        max_chunk_size = j["max_chunk_size"].get<int>();
    }
    
    if (j.contains("overlap"))
    {
        if (!j["overlap"].is_number_integer())
        {
            throw std::runtime_error("Field 'overlap' must be an integer");
        }
        overlap = j["overlap"].get<int>();
    }
    
    if (j.contains("similarity_threshold"))
    {
        if (!j["similarity_threshold"].is_number())
        {
            throw std::runtime_error("Field 'similarity_threshold' must be a number");
        }
        similarity_threshold = j["similarity_threshold"].get<float>();
    }
    
    if (j.contains("method"))
    {
        if (!j["method"].is_string())
        {
            throw std::runtime_error("Field 'method' must be a string");
        }
        method = j["method"].get<std::string>();
    }
}

} // namespace kolosal

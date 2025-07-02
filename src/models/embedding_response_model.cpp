#include "kolosal/models/embedding_response_model.hpp"

namespace kolosal
{

// EmbeddingData implementations
nlohmann::json EmbeddingData::to_json() const
{
    nlohmann::json j;
    j["object"] = object;
    j["embedding"] = embedding;
    j["index"] = index;
    return j;
}

void EmbeddingData::from_json(const nlohmann::json& j)
{
    if (j.contains("object"))
    {
        object = j["object"];
    }
      if (j.contains("embedding"))
    {
        embedding = j["embedding"].get<std::vector<float>>();
    }
    
    if (j.contains("index"))
    {
        index = j["index"];
    }
}

// EmbeddingUsage implementations
nlohmann::json EmbeddingUsage::to_json() const
{
    nlohmann::json j;
    j["prompt_tokens"] = prompt_tokens;
    j["total_tokens"] = total_tokens;
    return j;
}

void EmbeddingUsage::from_json(const nlohmann::json& j)
{
    if (j.contains("prompt_tokens"))
    {
        prompt_tokens = j["prompt_tokens"];
    }
    
    if (j.contains("total_tokens"))
    {
        total_tokens = j["total_tokens"];
    }
}

// EmbeddingResponse implementations
bool EmbeddingResponse::validate() const
{
    if (model.empty())
    {
        return false;
    }
    
    if (data.empty())
    {
        return false;
    }
    
    // Validate that all embedding data has valid embeddings
    for (const auto& embedding_data : data)
    {
        if (embedding_data.embedding.empty())
        {
            return false;
        }
    }
    
    return true;
}

nlohmann::json EmbeddingResponse::to_json() const
{
    nlohmann::json j;
    j["object"] = object;
    
    nlohmann::json data_array = nlohmann::json::array();
    for (const auto& embedding_data : data)
    {
        data_array.push_back(embedding_data.to_json());
    }
    j["data"] = data_array;
    
    j["model"] = model;
    j["usage"] = usage.to_json();
    
    return j;
}

void EmbeddingResponse::from_json(const nlohmann::json& j)
{
    if (j.contains("object"))
    {
        object = j["object"];
    }
    
    if (j.contains("data"))
    {
        data.clear();
        for (const auto& item : j["data"])
        {
            EmbeddingData embedding_data;
            embedding_data.from_json(item);
            data.push_back(embedding_data);
        }
    }
    
    if (j.contains("model"))
    {
        model = j["model"];
    }
    
    if (j.contains("usage"))
    {
        usage.from_json(j["usage"]);
    }
}

void EmbeddingResponse::addEmbedding(const std::vector<float>& embedding, int index)
{
    EmbeddingData embedding_data;
    embedding_data.embedding = embedding;
    embedding_data.index = index;
    data.push_back(embedding_data);
}

void EmbeddingResponse::setUsage(int prompt_tokens)
{
    usage.prompt_tokens = prompt_tokens;
    usage.total_tokens = prompt_tokens; // For embeddings, total == prompt tokens
}

// EmbeddingErrorResponse implementations
nlohmann::json EmbeddingErrorResponse::ErrorDetails::to_json() const
{
    nlohmann::json j;
    j["message"] = message;
    j["type"] = type;
    j["param"] = param;
    j["code"] = code;
    return j;
}

void EmbeddingErrorResponse::ErrorDetails::from_json(const nlohmann::json& j)
{
    if (j.contains("message"))
    {
        message = j["message"];
    }
    
    if (j.contains("type"))
    {
        type = j["type"];
    }
    
    if (j.contains("param"))
    {
        param = j["param"];
    }
    
    if (j.contains("code"))
    {
        code = j["code"];
    }
}

bool EmbeddingErrorResponse::validate() const
{
    return !error.message.empty();
}

nlohmann::json EmbeddingErrorResponse::to_json() const
{
    nlohmann::json j;
    j["error"] = error.to_json();
    return j;
}

void EmbeddingErrorResponse::from_json(const nlohmann::json& j)
{
    if (j.contains("error"))
    {
        error.from_json(j["error"]);
    }
}

} // namespace kolosal

#include "kolosal/models/chunking_response_model.hpp"
#include "kolosal/logger.hpp"
#include <stdexcept>

namespace kolosal
{

nlohmann::json ChunkData::to_json() const
{
    nlohmann::json j;
    j["text"] = text;
    j["index"] = index;
    j["token_count"] = token_count;
    return j;
}

bool ChunkingResponse::validate() const
{
    if (model_name.empty())
    {
        ServerLogger::logDebug("Validation failed: model_name is empty");
        return false;
    }
    
    if (method != "regular" && method != "semantic")
    {
        ServerLogger::logDebug("Validation failed: method must be 'regular' or 'semantic', got '%s'", method.c_str());
        return false;
    }
    
    if (total_chunks != static_cast<int>(chunks.size()))
    {
        ServerLogger::logDebug("Validation failed: total_chunks (%d) doesn't match chunks.size() (%zu)", total_chunks, chunks.size());
        return false;
    }
    
    if (usage.original_tokens < 0 || usage.total_chunk_tokens < 0)
    {
        ServerLogger::logDebug("Validation failed: token counts cannot be negative");
        return false;
    }
    
    return true;
}

nlohmann::json ChunkingResponse::to_json() const
{
    nlohmann::json j;
    j["model_name"] = model_name;
    j["method"] = method;
    j["total_chunks"] = total_chunks;
    
    nlohmann::json chunks_array = nlohmann::json::array();
    for (const auto& chunk : chunks)
    {
        chunks_array.push_back(chunk.to_json());
    }
    j["chunks"] = chunks_array;
    
    nlohmann::json usage_json;
    usage_json["original_tokens"] = usage.original_tokens;
    usage_json["total_chunk_tokens"] = usage.total_chunk_tokens;
    usage_json["processing_time_ms"] = usage.processing_time_ms;
    j["usage"] = usage_json;
    
    return j;
}

void ChunkingResponse::from_json(const nlohmann::json& j)
{
    if (j.contains("model_name") && j["model_name"].is_string())
    {
        model_name = j["model_name"].get<std::string>();
    }
    
    if (j.contains("method") && j["method"].is_string())
    {
        method = j["method"].get<std::string>();
    }
    
    if (j.contains("total_chunks") && j["total_chunks"].is_number_integer())
    {
        total_chunks = j["total_chunks"].get<int>();
    }
    
    if (j.contains("chunks") && j["chunks"].is_array())
    {
        chunks.clear();
        for (const auto& chunk_json : j["chunks"])
        {
            if (chunk_json.contains("text") && chunk_json.contains("index") && chunk_json.contains("token_count"))
            {
                ChunkData chunk;
                chunk.text = chunk_json["text"].get<std::string>();
                chunk.index = chunk_json["index"].get<int>();
                chunk.token_count = chunk_json["token_count"].get<int>();
                chunks.push_back(chunk);
            }
        }
    }
    
    if (j.contains("usage") && j["usage"].is_object())
    {
        const auto& usage_json = j["usage"];
        if (usage_json.contains("original_tokens") && usage_json["original_tokens"].is_number_integer())
        {
            usage.original_tokens = usage_json["original_tokens"].get<int>();
        }
        if (usage_json.contains("total_chunk_tokens") && usage_json["total_chunk_tokens"].is_number_integer())
        {
            usage.total_chunk_tokens = usage_json["total_chunk_tokens"].get<int>();
        }
        if (usage_json.contains("processing_time_ms") && usage_json["processing_time_ms"].is_number())
        {
            usage.processing_time_ms = usage_json["processing_time_ms"].get<float>();
        }
    }
}

void ChunkingResponse::addChunk(const ChunkData& chunk)
{
    chunks.push_back(chunk);
    total_chunks = static_cast<int>(chunks.size());
}

void ChunkingResponse::setUsage(int original_tokens, int total_chunk_tokens, float processing_time_ms)
{
    usage.original_tokens = original_tokens;
    usage.total_chunk_tokens = total_chunk_tokens;
    usage.processing_time_ms = processing_time_ms;
}

} // namespace kolosal

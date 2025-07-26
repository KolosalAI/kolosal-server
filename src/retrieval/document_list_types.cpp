#include "kolosal/retrieval/document_list_types.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <algorithm>

namespace kolosal
{
namespace retrieval
{

nlohmann::json ListDocumentsResponse::to_json() const
{
    nlohmann::json j;
    j["document_ids"] = document_ids;
    j["total_count"] = total_count;
    j["collection_name"] = collection_name;
    return j;
}

nlohmann::json DocumentInfo::to_json() const
{
    nlohmann::json j;
    j["id"] = id;
    j["text"] = text;
    j["metadata"] = nlohmann::json::object();
    
    for (const auto& [key, value] : metadata)
    {
        j["metadata"][key] = value;
    }
    
    return j;
}

void DocumentsInfoRequest::from_json(const nlohmann::json& j)
{
    try
    {
        if (!j.contains("ids") || !j["ids"].is_array())
        {
            throw std::runtime_error("Missing or invalid 'ids' field - must be an array of strings");
        }
        
        ids.clear();
        for (const auto& id_json : j["ids"])
        {
            if (!id_json.is_string())
            {
                throw std::runtime_error("All IDs must be strings");
            }
            ids.push_back(id_json.get<std::string>());
        }
    }
    catch (const nlohmann::json::exception& ex)
    {
        throw std::runtime_error("JSON parsing error: " + std::string(ex.what()));
    }
}

bool DocumentsInfoRequest::validate() const
{
    if (ids.empty())
    {
        ServerLogger::logWarning("DocumentsInfoRequest validation failed: empty ids array");
        return false;
    }
    
    // Check for empty or whitespace-only IDs
    for (const auto& id : ids)
    {
        if (id.empty() || std::all_of(id.begin(), id.end(), ::isspace))
        {
            ServerLogger::logWarning("DocumentsInfoRequest validation failed: empty or whitespace-only ID");
            return false;
        }
    }
    
    return true;
}

nlohmann::json DocumentsInfoResponse::to_json() const
{
    nlohmann::json j;
    
    j["documents"] = nlohmann::json::array();
    for (const auto& doc : documents)
    {
        j["documents"].push_back(doc.to_json());
    }
    
    j["found_count"] = found_count;
    j["not_found_count"] = not_found_count;
    j["not_found_ids"] = not_found_ids;
    j["collection_name"] = collection_name;
    
    return j;
}

nlohmann::json DocumentsErrorResponse::to_json() const
{
    nlohmann::json j;
    j["error"] = error;
    j["error_type"] = error_type;
    if (!param.empty())
    {
        j["param"] = param;
    }
    if (!code.empty())
    {
        j["code"] = code;
    }
    return j;
}

} // namespace retrieval
} // namespace kolosal

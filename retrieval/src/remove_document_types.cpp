#include "kolosal/retrieval/remove_document_types.hpp"
#include <stdexcept>

namespace kolosal
{
namespace retrieval
{

// RemoveDocumentsRequest implementations
void RemoveDocumentsRequest::from_json(const nlohmann::json& j)
{
    if (!j.is_object())
    {
        throw std::runtime_error("Request must be a JSON object");
    }
    
    if (!j.contains("document_ids"))
    {
        throw std::runtime_error("Missing required field: document_ids");
    }
    
    if (!j["document_ids"].is_array())
    {
        throw std::runtime_error("Field 'document_ids' must be an array");
    }
    
    ids.clear();
    for (const auto& id_item : j["document_ids"])
    {
        if (!id_item.is_string())
        {
            throw std::runtime_error("All elements in 'document_ids' must be strings");
        }
        ids.push_back(id_item.get<std::string>());
    }
    
    // Collection name is always "documents" - ignore any provided collection_name
    collection_name = "documents";
}

bool RemoveDocumentsRequest::validate() const
{
    if (ids.empty())
    {
        return false;
    }
    
    // Check that all document IDs are non-empty
    for (const auto& id : ids)
    {
        if (id.empty())
        {
            return false;
        }
    }
    
    return true;
}

// DocumentRemovalResult implementations
nlohmann::json DocumentRemovalResult::to_json() const
{
    nlohmann::json j;
    j["id"] = id;
    j["status"] = status;
    return j;
}

// RemoveDocumentsResponse implementations
void RemoveDocumentsResponse::addRemoved(const std::string& document_id)
{
    DocumentRemovalResult result;
    result.id = document_id;
    result.status = "removed";
    results.push_back(result);
    removed_count++;
}

void RemoveDocumentsResponse::addFailed(const std::string& document_id)
{
    DocumentRemovalResult result;
    result.id = document_id;
    result.status = "failed";
    results.push_back(result);
    failed_count++;
}

void RemoveDocumentsResponse::addNotFound(const std::string& document_id)
{
    DocumentRemovalResult result;
    result.id = document_id;
    result.status = "not_found";
    results.push_back(result);
    not_found_count++;
}

nlohmann::json RemoveDocumentsResponse::to_json() const
{
    nlohmann::json j;
    j["collection_name"] = collection_name;
    j["removed_count"] = removed_count;
    j["failed_count"] = failed_count;
    j["not_found_count"] = not_found_count;
    j["results"] = nlohmann::json::array();
    
    for (const auto& result : results)
    {
        j["results"].push_back(result.to_json());
    }
    
    return j;
}

bool RemoveDocumentsResponse::validate() const
{
    return !collection_name.empty();
}

// RemoveDocumentsErrorResponse implementations
nlohmann::json RemoveDocumentsErrorResponse::to_json() const
{
    nlohmann::json j;
    nlohmann::json error_obj;
    error_obj["message"] = error;
    error_obj["type"] = error_type;
    
    if (!param.empty())
    {
        error_obj["param"] = param;
    }
    
    if (!code.empty())
    {
        error_obj["code"] = code;
    }
    
    j["error"] = error_obj;
    return j;
}

} // namespace retrieval
} // namespace kolosal

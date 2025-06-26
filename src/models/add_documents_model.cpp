#include "kolosal/models/add_documents_model.hpp"
#include <stdexcept>

namespace kolosal
{

// Document implementations
nlohmann::json Document::to_json() const
{
    nlohmann::json j;
    j["text"] = text;
    j["metadata"] = metadata;
    return j;
}

void Document::from_json(const nlohmann::json& j)
{
    if (j.contains("text") && j["text"].is_string())
    {
        text = j["text"];
    }
    else
    {
        throw std::runtime_error("Document must contain a 'text' field of type string");
    }
    
    if (j.contains("metadata"))
    {
        if (j["metadata"].is_object())
        {
            metadata.clear();
            for (auto& [key, value] : j["metadata"].items())
            {
                metadata[key] = value;
            }
        }
        else
        {
            throw std::runtime_error("Document 'metadata' field must be an object");
        }
    }
}

bool Document::validate() const
{
    return !text.empty();
}

// AddDocumentsRequest implementations
void AddDocumentsRequest::from_json(const nlohmann::json& j)
{
    if (!j.contains("documents") || !j["documents"].is_array())
    {
        throw std::runtime_error("Request must contain a 'documents' array");
    }
    
    documents.clear();
    for (const auto& doc_json : j["documents"])
    {
        Document doc;
        doc.from_json(doc_json);
        documents.push_back(doc);
    }
    
    if (j.contains("collection_name") && j["collection_name"].is_string())
    {
        collection_name = j["collection_name"];
    }
}

bool AddDocumentsRequest::validate() const
{
    if (documents.empty())
    {
        return false;
    }
    
    for (const auto& doc : documents)
    {
        if (!doc.validate())
        {
            return false;
        }
    }
    
    return true;
}

// DocumentResult implementations
nlohmann::json DocumentResult::to_json() const
{
    nlohmann::json j;
    j["id"] = id;
    j["success"] = success;
    if (!success && !error.empty())
    {
        j["error"] = error;
    }
    return j;
}

// AddDocumentsResponse implementations
void AddDocumentsResponse::addSuccess(const std::string& document_id)
{
    DocumentResult result;
    result.id = document_id;
    result.success = true;
    results.push_back(result);
    successful_count++;
}

void AddDocumentsResponse::addFailure(const std::string& error)
{
    DocumentResult result;
    result.success = false;
    result.error = error;
    results.push_back(result);
    failed_count++;
}

nlohmann::json AddDocumentsResponse::to_json() const
{
    nlohmann::json j;
    j["collection_name"] = collection_name;
    j["successful_count"] = successful_count;
    j["failed_count"] = failed_count;
    
    nlohmann::json results_array = nlohmann::json::array();
    for (const auto& result : results)
    {
        results_array.push_back(result.to_json());
    }
    j["results"] = results_array;
    
    return j;
}

bool AddDocumentsResponse::validate() const
{
    return successful_count >= 0 && failed_count >= 0 && 
           (successful_count + failed_count) == static_cast<int>(results.size());
}

// AddDocumentsErrorResponse implementations
nlohmann::json AddDocumentsErrorResponse::to_json() const
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

} // namespace kolosal

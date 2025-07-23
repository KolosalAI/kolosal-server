#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/logger.hpp"
#include <stdexcept>

namespace kolosal
{
namespace retrieval
{

// RetrieveRequest implementation
void RetrieveRequest::from_json(const nlohmann::json& j)
{
    if (!j.contains("query") || !j["query"].is_string())
    {
        throw std::runtime_error("Missing or invalid 'query' field - must be a string");
    }
    
    query = j["query"].get<std::string>();
    
    if (j.contains("k"))
    {
        if (!j["k"].is_number_integer() || j["k"].get<int>() <= 0)
        {
            throw std::runtime_error("Field 'k' must be a positive integer");
        }
        k = j["k"].get<int>();
    }
    
    if (j.contains("collection_name"))
    {
        if (!j["collection_name"].is_string())
        {
            throw std::runtime_error("Field 'collection_name' must be a string");
        }
        collection_name = j["collection_name"].get<std::string>();
    }
    
    if (j.contains("score_threshold"))
    {
        if (!j["score_threshold"].is_number())
        {
            throw std::runtime_error("Field 'score_threshold' must be a number");
        }
        score_threshold = j["score_threshold"].get<float>();
    }
}

bool RetrieveRequest::validate() const
{
    if (query.empty())
    {
        ServerLogger::logWarning("Validation failed: query is empty - please provide a search query");
        return false;
    }
    
    if (query.length() > 10000)  // Reasonable query length limit
    {
        ServerLogger::logWarning("Validation failed: query too long (%zu chars) - maximum 10000 characters allowed", query.length());
        return false;
    }
    
    if (k <= 0 || k > 1000)  // Reasonable upper limit
    {
        ServerLogger::logWarning("Validation failed: k must be between 1 and 1000, got %d", k);
        return false;
    }
    
    if (score_threshold < 0.0f || score_threshold > 1.0f)
    {
        ServerLogger::logWarning("Validation failed: score_threshold must be between 0.0 and 1.0, got %f", score_threshold);
        return false;
    }
    
    // Log validation success with parameters for debugging
    ServerLogger::logDebug("Request validation successful - query length: %zu, k: %d, threshold: %.3f", 
                          query.length(), k, score_threshold);
    
    return true;
}

// RetrievedDocument implementation
nlohmann::json RetrievedDocument::to_json() const
{
    nlohmann::json j;
    j["id"] = id;
    j["text"] = text;
    j["metadata"] = metadata;
    j["score"] = score;
    return j;
}

// RetrieveResponse implementation
void RetrieveResponse::addDocument(const RetrievedDocument& document)
{
    documents.push_back(document);
    total_found = static_cast<int>(documents.size());
}

nlohmann::json RetrieveResponse::to_json() const
{
    nlohmann::json j;
    j["query"] = query;
    j["k"] = k;
    j["collection_name"] = collection_name;
    j["score_threshold"] = score_threshold;
    j["total_found"] = total_found;
    
    nlohmann::json docs_array = nlohmann::json::array();
    for (const auto& doc : documents)
    {
        docs_array.push_back(doc.to_json());
    }
    j["documents"] = docs_array;
    
    return j;
}

bool RetrieveResponse::validate() const
{
    if (query.empty())
    {
        return false;
    }
    
    if (k <= 0)
    {
        return false;
    }
    
    if (total_found < 0)
    {
        return false;
    }
    
    return true;
}

// RetrieveErrorResponse implementation
nlohmann::json RetrieveErrorResponse::to_json() const
{
    nlohmann::json j;
    j["error"] = error;
    j["error_type"] = error_type;
    j["param"] = param;
    j["code"] = code;
    return j;
}

} // namespace retrieval
} // namespace kolosal

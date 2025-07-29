#pragma once

#include "kolosal/agents/agent_interfaces.hpp"
#include "kolosal/retrieval/add_document_types.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include <json.hpp>
#include <memory>
#include <future>
#include <vector>
#include <string>

namespace kolosal::agents {

using json = nlohmann::json;

// Forward declarations for types that may not exist yet
struct QueryResult {
    std::string query;
    bool success = false;
    std::string error;
    std::string error_type;
    size_t total_found = 0;
    std::vector<kolosal::retrieval::RetrievedDocument> documents;
};

struct DocumentResult {
    std::string id;
    bool success = false;
    std::string error;
};

class DocumentAgentService {
public:
    DocumentAgentService() = default;
    ~DocumentAgentService() = default;

    // Bulk document operations
    struct BulkDocumentRequest {
        std::vector<kolosal::retrieval::Document> documents;
        std::string collection_name = "documents";
        int batch_size = 100;
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct BulkDocumentResponse {
        bool success = false;
        std::string message;
        size_t total_documents = 0;
        size_t successful_count = 0;
        size_t failed_count = 0;
        std::string collection_name;
        std::vector<DocumentResult> results;
        
        json to_json() const;
    };

    // Bulk retrieval operations
    struct BulkRetrievalRequest {
        std::vector<std::string> queries;
        int k = 5;
        double score_threshold = 0.0;
        std::string collection_name = "documents";
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct BulkRetrievalResponse {
        bool success = false;
        std::string message;
        size_t total_queries = 0;
        std::vector<QueryResult> results;
        
        json to_json() const;
    };

    // Document search operations
    struct DocumentSearchRequest {
        std::string query;
        std::string collection_name = "documents";
        int limit = 10;
        double score_threshold = 0.0;
        json filters;
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct DocumentSearchResponse {
        bool success = false;
        std::string message;
        std::vector<kolosal::retrieval::RetrievedDocument> documents;
        size_t total_found = 0;
        
        json to_json() const;
    };

    // Document upload operations
    struct DocumentUploadRequest {
        std::string content;
        std::string filename;
        std::string collection_name = "documents";
        json metadata;
        bool chunk_document = true;
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct DocumentUploadResponse {
        bool success = false;
        std::string message;
        std::string document_id;
        std::vector<std::string> chunk_ids;
        
        json to_json() const;
    };

    // Core service methods
    std::future<BulkDocumentResponse> processBulkDocuments(const BulkDocumentRequest& request);
    std::future<BulkRetrievalResponse> processBulkRetrieval(const BulkRetrievalRequest& request);
    std::future<DocumentSearchResponse> searchDocuments(const DocumentSearchRequest& request);
    std::future<DocumentUploadResponse> uploadDocument(const DocumentUploadRequest& request);

    // Collection management
    std::future<json> listCollections();
    std::future<json> createCollection(const std::string& name, const json& config = {});
    std::future<json> deleteCollection(const std::string& name);
    std::future<json> getCollectionInfo(const std::string& name);

private:
    kolosal::retrieval::DocumentService& getDocumentService();
    std::string generateErrorMessage(const std::string& operation, const std::exception& e);
};

} // namespace kolosal::agents

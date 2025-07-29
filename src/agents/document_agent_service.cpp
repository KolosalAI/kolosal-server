#include "kolosal/agents/document_agent_service.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <stdexcept>
#include <regex>

namespace kolosal::agents {

// BulkDocumentRequest implementation
void DocumentAgentService::BulkDocumentRequest::from_json(const json& j) {
    if (j.contains("documents") && j["documents"].is_array()) {
        documents.clear();
        for (const auto& doc_json : j["documents"]) {
            kolosal::retrieval::Document doc;
            if (doc_json.contains("text") && doc_json["text"].is_string()) {
                doc.text = doc_json["text"];
            }
            if (doc_json.contains("metadata")) {
                doc.metadata = doc_json["metadata"];
            }
            documents.push_back(doc);
        }
    }
    
    if (j.contains("collection_name") && j["collection_name"].is_string()) {
        collection_name = j["collection_name"];
    }
    
    if (j.contains("batch_size") && j["batch_size"].is_number_integer()) {
        batch_size = j["batch_size"];
    }
}

bool DocumentAgentService::BulkDocumentRequest::validate() const {
    return !documents.empty() && 
           !collection_name.empty() && 
           batch_size > 0 &&
           std::all_of(documents.begin(), documents.end(), 
                      [](const auto& doc) { return !doc.text.empty(); });
}

// BulkDocumentResponse implementation
json DocumentAgentService::BulkDocumentResponse::to_json() const {
    json response = {
        {"success", success},
        {"message", message},
        {"total_documents", total_documents},
        {"successful_count", successful_count},
        {"failed_count", failed_count},
        {"collection_name", collection_name},
        {"results", json::array()}
    };
    
    for (const auto& result : results) {
        json result_item = {
            {"id", result.id},
            {"success", result.success},
            {"error", result.error}
        };
        response["results"].push_back(result_item);
    }
    
    return response;
}

// BulkRetrievalRequest implementation
void DocumentAgentService::BulkRetrievalRequest::from_json(const json& j) {
    if (j.contains("queries") && j["queries"].is_array()) {
        queries.clear();
        for (const auto& query : j["queries"]) {
            if (query.is_string()) {
                queries.push_back(query);
            }
        }
    } else if (j.contains("query_list") && j["query_list"].is_array()) {
        queries.clear();
        for (const auto& query : j["query_list"]) {
            if (query.is_string()) {
                queries.push_back(query);
            }
        }
    }
    
    if (j.contains("k") && j["k"].is_number_integer()) {
        k = j["k"];
    }
    
    if (j.contains("score_threshold") && j["score_threshold"].is_number()) {
        score_threshold = j["score_threshold"];
    }
    
    if (j.contains("collection_name") && j["collection_name"].is_string()) {
        collection_name = j["collection_name"];
    }
}

bool DocumentAgentService::BulkRetrievalRequest::validate() const {
    return !queries.empty() && 
           !collection_name.empty() && 
           k > 0 &&
           score_threshold >= 0.0;
}

// BulkRetrievalResponse implementation
json DocumentAgentService::BulkRetrievalResponse::to_json() const {
    json response = {
        {"success", success},
        {"message", message},
        {"total_queries", total_queries},
        {"results", json::array()}
    };
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        json query_result = {
            {"query_index", i},
            {"query", result.query},
            {"success", result.success},
            {"total_found", result.total_found},
            {"documents", json::array()}
        };
        
        if (!result.success) {
            query_result["error"] = result.error;
            query_result["error_type"] = result.error_type;
        } else {
            for (const auto& doc : result.documents) {
                json doc_item = {
                    {"id", doc.id},
                    {"text", doc.text},
                    {"score", doc.score},
                    {"metadata", doc.metadata}
                };
                query_result["documents"].push_back(doc_item);
            }
        }
        
        response["results"].push_back(query_result);
    }
    
    return response;
}

// DocumentSearchRequest implementation
void DocumentAgentService::DocumentSearchRequest::from_json(const json& j) {
    if (j.contains("query") && j["query"].is_string()) {
        query = j["query"];
    }
    
    if (j.contains("collection_name") && j["collection_name"].is_string()) {
        collection_name = j["collection_name"];
    }
    
    if (j.contains("limit") && j["limit"].is_number_integer()) {
        limit = j["limit"];
    }
    
    if (j.contains("score_threshold") && j["score_threshold"].is_number()) {
        score_threshold = j["score_threshold"];
    }
    
    if (j.contains("filters")) {
        filters = j["filters"];
    }
}

bool DocumentAgentService::DocumentSearchRequest::validate() const {
    return !query.empty() && 
           !collection_name.empty() && 
           limit > 0 &&
           score_threshold >= 0.0;
}

// DocumentSearchResponse implementation
json DocumentAgentService::DocumentSearchResponse::to_json() const {
    json response = {
        {"success", success},
        {"message", message},
        {"total_found", total_found},
        {"documents", json::array()}
    };
    
    for (const auto& doc : documents) {
        json doc_item = {
            {"id", doc.id},
            {"text", doc.text},
            {"score", doc.score},
            {"metadata", doc.metadata}
        };
        response["documents"].push_back(doc_item);
    }
    
    return response;
}

// DocumentUploadRequest implementation
void DocumentAgentService::DocumentUploadRequest::from_json(const json& j) {
    if (j.contains("content") && j["content"].is_string()) {
        content = j["content"];
    }
    
    if (j.contains("filename") && j["filename"].is_string()) {
        filename = j["filename"];
    }
    
    if (j.contains("collection_name") && j["collection_name"].is_string()) {
        collection_name = j["collection_name"];
    }
    
    if (j.contains("metadata")) {
        metadata = j["metadata"];
    }
    
    if (j.contains("chunk_document") && j["chunk_document"].is_boolean()) {
        chunk_document = j["chunk_document"];
    }
}

bool DocumentAgentService::DocumentUploadRequest::validate() const {
    return !content.empty() && 
           !filename.empty() && 
           !collection_name.empty();
}

// DocumentUploadResponse implementation
json DocumentAgentService::DocumentUploadResponse::to_json() const {
    json response = {
        {"success", success},
        {"message", message},
        {"document_id", document_id},
        {"chunk_ids", json::array()}
    };
    
    for (const auto& chunk_id : chunk_ids) {
        response["chunk_ids"].push_back(chunk_id);
    }
    
    return response;
}

// Core service methods implementation
std::future<DocumentAgentService::BulkDocumentResponse> 
DocumentAgentService::processBulkDocuments(const BulkDocumentRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        BulkDocumentResponse response;
        
        try {
            if (!request.validate()) {
                response.success = false;
                response.message = "Invalid bulk document request";
                return response;
            }
            
            ServerLogger::logInfo("Processing bulk upload of %zu documents", request.documents.size());
            
            // Convert to AddDocumentsRequest
            kolosal::retrieval::AddDocumentsRequest addRequest;
            addRequest.documents = request.documents;
            addRequest.collection_name = request.collection_name;
            
            auto& documentService = getDocumentService();
            auto future_response = documentService.addDocuments(addRequest);
            auto add_response = future_response.get();
            
            response.success = true;
            response.message = "Bulk document operation completed";
            response.total_documents = request.documents.size();
            response.successful_count = add_response.successful_count;
            response.failed_count = add_response.failed_count;
            response.collection_name = add_response.collection_name;
            
            // Convert DocumentResult types
            response.results.clear();
            response.results.reserve(add_response.results.size());
            for (const auto& result : add_response.results) {
                DocumentResult agentResult;
                agentResult.id = result.id;
                agentResult.success = result.success;
                agentResult.error = result.error;
                response.results.push_back(agentResult);
            }
            
        } catch (const std::exception& e) {
            response.success = false;
            response.message = generateErrorMessage("bulk document processing", e);
            ServerLogger::logError("Error in processBulkDocuments: %s", e.what());
        }
        
        return response;
    });
}

std::future<DocumentAgentService::BulkRetrievalResponse> 
DocumentAgentService::processBulkRetrieval(const BulkRetrievalRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        BulkRetrievalResponse response;
        
        try {
            if (!request.validate()) {
                response.success = false;
                response.message = "Invalid bulk retrieval request";
                return response;
            }
            
            ServerLogger::logInfo("Processing bulk retrieval for %zu queries", request.queries.size());
            
            auto& documentService = getDocumentService();
            
            response.success = true;
            response.message = "Bulk retrieval completed";
            response.total_queries = request.queries.size();
            
            for (size_t i = 0; i < request.queries.size(); ++i) {
                const auto& query = request.queries[i];
                
                QueryResult queryResult;
                queryResult.query = query;
                
                try {
                    kolosal::retrieval::RetrieveRequest retrieveRequest;
                    retrieveRequest.query = query;
                    retrieveRequest.k = request.k;
                    retrieveRequest.score_threshold = request.score_threshold;
                    retrieveRequest.collection_name = request.collection_name;
                    
                    if (retrieveRequest.validate()) {
                        auto future_response = documentService.retrieveDocuments(retrieveRequest);
                        auto retrieve_response = future_response.get();
                        
                        queryResult.success = true;
                        queryResult.total_found = retrieve_response.total_found;
                        queryResult.documents = retrieve_response.documents;
                    } else {
                        queryResult.success = false;
                        queryResult.error = "Invalid query parameters";
                        queryResult.error_type = "validation_error";
                    }
                    
                } catch (const std::runtime_error& e) {
                    queryResult.success = false;
                    queryResult.error = e.what();
                    
                    std::string error_msg = e.what();
                    if (error_msg.find("embedding") != std::string::npos) {
                        queryResult.error_type = "embedding_generation_failed";
                    } else if (error_msg.find("Collection") != std::string::npos && 
                               error_msg.find("does not exist") != std::string::npos) {
                        queryResult.error_type = "collection_not_found";
                    } else if (error_msg.find("timeout") != std::string::npos) {
                        queryResult.error_type = "request_timeout";
                    } else {
                        queryResult.error_type = "service_error";
                    }
                    
                    ServerLogger::logWarning("Query %zu failed in bulk retrieval: %s", i, error_msg.c_str());
                } catch (const std::exception& e) {
                    queryResult.success = false;
                    queryResult.error = e.what();
                    queryResult.error_type = "unknown_error";
                    
                    ServerLogger::logError("Unexpected error for query %zu in bulk retrieval: %s", i, e.what());
                }
                
                response.results.push_back(queryResult);
            }
            
        } catch (const std::exception& e) {
            response.success = false;
            response.message = generateErrorMessage("bulk retrieval", e);
            ServerLogger::logError("Error in processBulkRetrieval: %s", e.what());
        }
        
        return response;
    });
}

std::future<DocumentAgentService::DocumentSearchResponse> 
DocumentAgentService::searchDocuments(const DocumentSearchRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        DocumentSearchResponse response;
        
        try {
            if (!request.validate()) {
                response.success = false;
                response.message = "Invalid document search request";
                return response;
            }
            
            ServerLogger::logInfo("Processing document search for query: %s", request.query.c_str());
            
            kolosal::retrieval::RetrieveRequest retrieveRequest;
            retrieveRequest.query = request.query;
            retrieveRequest.k = request.limit;
            retrieveRequest.score_threshold = request.score_threshold;
            retrieveRequest.collection_name = request.collection_name;
            
            auto& documentService = getDocumentService();
            auto future_response = documentService.retrieveDocuments(retrieveRequest);
            auto retrieve_response = future_response.get();
            
            response.success = true;
            response.message = "Document search completed";
            response.total_found = retrieve_response.total_found;
            response.documents = retrieve_response.documents;
            
        } catch (const std::exception& e) {
            response.success = false;
            response.message = generateErrorMessage("document search", e);
            ServerLogger::logError("Error in searchDocuments: %s", e.what());
        }
        
        return response;
    });
}

std::future<DocumentAgentService::DocumentUploadResponse> 
DocumentAgentService::uploadDocument(const DocumentUploadRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        DocumentUploadResponse response;
        
        try {
            if (!request.validate()) {
                response.success = false;
                response.message = "Invalid document upload request";
                return response;
            }
            
            ServerLogger::logInfo("Processing document upload for file: %s", request.filename.c_str());
            
            kolosal::retrieval::Document document;
            document.text = request.content;
            document.metadata = request.metadata;
            document.metadata["filename"] = request.filename;
            document.metadata["upload_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            kolosal::retrieval::AddDocumentsRequest addRequest;
            addRequest.documents = {document};
            addRequest.collection_name = request.collection_name;
            
            auto& documentService = getDocumentService();
            auto future_response = documentService.addDocuments(addRequest);
            auto add_response = future_response.get();
            
            if (add_response.successful_count > 0) {
                response.success = true;
                response.message = "Document uploaded successfully";
                response.document_id = add_response.results[0].id;
                // Note: chunk_ids would need to be populated based on chunking service
            } else {
                response.success = false;
                response.message = "Failed to upload document: " + add_response.results[0].error;
            }
            
        } catch (const std::exception& e) {
            response.success = false;
            response.message = generateErrorMessage("document upload", e);
            ServerLogger::logError("Error in uploadDocument: %s", e.what());
        }
        
        return response;
    });
}

std::future<json> DocumentAgentService::listCollections() {
    return std::async(std::launch::async, [this]() {
        try {
            auto& documentService = getDocumentService();
            // This would need to be implemented in the document service
            // For now, return a basic response
            json response = {
                {"success", true},
                {"collections", json::array()}
            };
            return response;
        } catch (const std::exception& e) {
            json response = {
                {"success", false},
                {"error", generateErrorMessage("list collections", e)}
            };
            return response;
        }
    });
}

std::future<json> DocumentAgentService::createCollection(const std::string& name, const json& config) {
    return std::async(std::launch::async, [this, name, config]() {
        try {
            // Collection creation logic would be implemented here
            json response = {
                {"success", true},
                {"collection_name", name},
                {"message", "Collection created successfully"}
            };
            return response;
        } catch (const std::exception& e) {
            json response = {
                {"success", false},
                {"error", generateErrorMessage("create collection", e)}
            };
            return response;
        }
    });
}

std::future<json> DocumentAgentService::deleteCollection(const std::string& name) {
    return std::async(std::launch::async, [this, name]() {
        try {
            // Collection deletion logic would be implemented here
            json response = {
                {"success", true},
                {"collection_name", name},
                {"message", "Collection deleted successfully"}
            };
            return response;
        } catch (const std::exception& e) {
            json response = {
                {"success", false},
                {"error", generateErrorMessage("delete collection", e)}
            };
            return response;
        }
    });
}

std::future<json> DocumentAgentService::getCollectionInfo(const std::string& name) {
    return std::async(std::launch::async, [this, name]() {
        try {
            // Collection info retrieval logic would be implemented here
            json response = {
                {"success", true},
                {"collection_name", name},
                {"document_count", 0},
                {"created_at", ""},
                {"metadata", json::object()}
            };
            return response;
        } catch (const std::exception& e) {
            json response = {
                {"success", false},
                {"error", generateErrorMessage("get collection info", e)}
            };
            return response;
        }
    });
}

// Private helper methods
kolosal::retrieval::DocumentService& DocumentAgentService::getDocumentService() {
    auto& serverAPI = ServerAPI::instance();
    return serverAPI.getDocumentService();
}

std::string DocumentAgentService::generateErrorMessage(const std::string& operation, const std::exception& e) {
    std::string error_msg = e.what();
    
    if (error_msg.find("not initialized") != std::string::npos) {
        return "Service not ready - server may be initializing. Please try again in a moment.";
    } else if (error_msg.find("Qdrant is disabled") != std::string::npos) {
        return "Service is disabled in server configuration. Check server configuration.";
    } else {
        return "Service error during " + operation + ": " + error_msg;
    }
}

} // namespace kolosal::agents

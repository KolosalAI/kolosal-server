#include "kolosal/routes/bulk_operations_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/add_document_types.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include <json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace kolosal::routes
{

BulkOperationsRoute::BulkOperationsRoute()
{
    ServerLogger::logInfo("BulkOperationsRoute initialized");
}

BulkOperationsRoute::~BulkOperationsRoute() = default;

bool BulkOperationsRoute::match(const std::string& method, const std::string& path)
{
    return method == "POST" && 
           (path == "/api/v1/documents/bulk" || 
            path == "/retrieve-bulk" ||
            path == "/bulk/documents" ||
            path == "/bulk/retrieve");
}

void BulkOperationsRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        // Parse request to determine the operation type
        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        json request_data;
        try
        {
            request_data = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Check if this is a bulk retrieval request
        if (request_data.contains("queries") || request_data.contains("query_list"))
        {
            handleBulkRetrieval(sock, body);
        }
        else
        {
            // Default to bulk document operations
            handleBulkDocuments(sock, body);
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in BulkOperationsRoute::handle: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void BulkOperationsRoute::handleBulkDocuments(SocketType sock, const std::string& body)
{
    try
    {
        json request_data = json::parse(body);
        
        ServerLogger::logInfo("Bulk documents operation request received");

        // Expected format:
        // {
        //   "documents": [
        //     {"text": "...", "metadata": {...}},
        //     {"text": "...", "metadata": {...}}
        //   ],
        //   "collection_name": "documents",
        //   "batch_size": 100
        // }

        if (!request_data.contains("documents"))
        {
            sendErrorResponse(sock, 400, "documents array is required", "missing_parameter", "documents");
            return;
        }

        // Parse as AddDocumentsRequest
        kolosal::retrieval::AddDocumentsRequest addRequest;
        addRequest.from_json(request_data);

        if (!addRequest.validate())
        {
            sendErrorResponse(sock, 400, "Invalid bulk document data");
            return;
        }

        ServerLogger::logInfo("Processing bulk upload of %zu documents", addRequest.documents.size());

        // Submit to document service
        try
        {
            auto& serverAPI = ServerAPI::instance();
            auto& documentService = serverAPI.getDocumentService();
            
            auto future_response = documentService.addDocuments(addRequest);
            auto add_response = future_response.get();
            
            // Create response
            json response = {
                {"success", true},
                {"message", "Bulk document operation completed"},
                {"total_documents", addRequest.documents.size()},
                {"successful_count", add_response.successful_count},
                {"failed_count", add_response.failed_count},
                {"collection_name", add_response.collection_name},
                {"results", json::array()}
            };

            // Add individual results
            for (const auto& result : add_response.results)
            {
                json result_item = {
                    {"id", result.id},
                    {"success", result.success},
                    {"error", result.error}
                };
                response["results"].push_back(result_item);
            }

            sendSuccessResponse(sock, response);
        }
        catch (const std::runtime_error& e)
        {
            ServerLogger::logError("DocumentService error in bulk operation: %s", e.what());
            sendErrorResponse(sock, 503, "Document service not available: " + std::string(e.what()));
            return;
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Error in handleBulkDocuments: %s", ex.what());
        sendErrorResponse(sock, 500, "Internal server error");
    }
}

void BulkOperationsRoute::handleBulkRetrieval(SocketType sock, const std::string& body)
{
    try
    {
        json request_data = json::parse(body);
        
        ServerLogger::logInfo("Bulk retrieval operation request received");

        // Expected format:
        // {
        //   "queries": ["query1", "query2", "query3"],
        //   "k": 5,
        //   "score_threshold": 0.6,
        //   "collection_name": "documents"
        // }

        std::vector<std::string> queries;
        if (request_data.contains("queries") && request_data["queries"].is_array())
        {
            for (const auto& query : request_data["queries"])
            {
                if (query.is_string())
                {
                    queries.push_back(query);
                }
            }
        }
        else if (request_data.contains("query_list") && request_data["query_list"].is_array())
        {
            for (const auto& query : request_data["query_list"])
            {
                if (query.is_string())
                {
                    queries.push_back(query);
                }
            }
        }

        if (queries.empty())
        {
            sendErrorResponse(sock, 400, "queries array is required and must contain at least one query", "missing_parameter", "queries");
            return;
        }

        int k = request_data.value("k", 5);
        double score_threshold = request_data.value("score_threshold", 0.0);
        std::string collection_name = request_data.value("collection_name", "documents");

        ServerLogger::logInfo("Processing bulk retrieval for %zu queries", queries.size());

        // Process each query
        try
        {
            auto& serverAPI = ServerAPI::instance();
            auto& documentService = serverAPI.getDocumentService();
            
            json response = {
                {"success", true},
                {"message", "Bulk retrieval completed"},
                {"total_queries", queries.size()},
                {"results", json::array()}
            };

            for (size_t i = 0; i < queries.size(); ++i)
            {
                const auto& query = queries[i];
                
                kolosal::retrieval::RetrieveRequest retrieveRequest;
                retrieveRequest.query = query;
                retrieveRequest.k = k;
                retrieveRequest.score_threshold = score_threshold;
                retrieveRequest.collection_name = collection_name;

                if (retrieveRequest.validate())
                {
                    try
                    {
                        auto future_response = documentService.retrieveDocuments(retrieveRequest);
                        auto retrieve_response = future_response.get();
                        
                        json query_result = {
                            {"query_index", i},
                            {"query", query},
                            {"success", true},
                            {"total_found", retrieve_response.total_found},
                            {"documents", json::array()}
                        };

                        for (const auto& doc : retrieve_response.documents)
                        {
                            json doc_item = {
                                {"id", doc.id},
                                {"text", doc.text},
                                {"score", doc.score},
                                {"metadata", doc.metadata}
                            };
                            query_result["documents"].push_back(doc_item);
                        }

                        response["results"].push_back(query_result);
                    }
                    catch (const std::runtime_error& e)
                    {
                        // Handle DocumentService specific errors for individual queries
                        std::string error_msg = e.what();
                        std::string error_type = "service_error";
                        
                        if (error_msg.find("embedding") != std::string::npos) {
                            error_type = "embedding_generation_failed";
                        } else if (error_msg.find("Collection") != std::string::npos && error_msg.find("does not exist") != std::string::npos) {
                            error_type = "collection_not_found";
                        } else if (error_msg.find("timeout") != std::string::npos) {
                            error_type = "request_timeout";
                        }
                        
                        json query_result = {
                            {"query_index", i},
                            {"query", query},
                            {"success", false},
                            {"error", error_msg},
                            {"error_type", error_type}
                        };
                        response["results"].push_back(query_result);
                        
                        ServerLogger::logWarning("Query %zu failed in bulk retrieval: %s", i, error_msg.c_str());
                    }
                    catch (const std::exception& e)
                    {
                        json query_result = {
                            {"query_index", i},
                            {"query", query},
                            {"success", false},
                            {"error", e.what()},
                            {"error_type", "unknown_error"}
                        };
                        response["results"].push_back(query_result);
                        
                        ServerLogger::logError("Unexpected error for query %zu in bulk retrieval: %s", i, e.what());
                    }
                }
                else
                {
                    json query_result = {
                        {"query_index", i},
                        {"query", query},
                        {"success", false},
                        {"error", "Invalid query parameters"}
                    };
                    response["results"].push_back(query_result);
                }
            }

            sendSuccessResponse(sock, response);
        }
        catch (const std::runtime_error& e)
        {
            // Handle DocumentService specific errors with more context
            std::string error_msg = e.what();
            ServerLogger::logError("DocumentService error in bulk retrieval: %s", error_msg.c_str());
            
            if (error_msg.find("not initialized") != std::string::npos) {
                sendErrorResponse(sock, 503, "Bulk retrieval service not ready - server may be initializing. Please try again in a moment.", "service_not_ready");
            } else if (error_msg.find("Qdrant is disabled") != std::string::npos) {
                sendErrorResponse(sock, 503, "Bulk retrieval is disabled in server configuration. Check server configuration.", "service_disabled");
            } else {
                sendErrorResponse(sock, 503, "Bulk retrieval service error: " + error_msg, "service_error");
            }
            return;
        }
    }
    catch (const json::exception& ex)
    {
        ServerLogger::logError("JSON processing error in bulk retrieval: %s", ex.what());
        sendErrorResponse(sock, 400, "Invalid request format: " + std::string(ex.what()), "json_processing_error");
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("Unexpected error in bulk retrieval: %s", ex.what());
        sendErrorResponse(sock, 500, "An unexpected error occurred during bulk retrieval", "internal_server_error");
    }
}

void BulkOperationsRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                          const std::string& error_type, const std::string& param)
{
    json error_response;
    error_response["success"] = false;
    error_response["error"] = {
        {"type", error_type},
        {"message", message},
        {"code", status}
    };
    if (!param.empty())
    {
        error_response["error"]["param"] = param;
    }

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, error_response.dump(), headers);
}

void BulkOperationsRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data)
{
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, 200, data.dump(), headers);
}

} // namespace kolosal::routes

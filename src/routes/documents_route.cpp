#include "kolosal/routes/documents_route.hpp"
#include "kolosal/retrieval/add_document_types.hpp"
#include "kolosal/retrieval/remove_document_types.hpp"
#include "kolosal/retrieval/document_list_types.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <memory>

using json = nlohmann::json;

namespace kolosal
{

std::atomic<long long> DocumentsRoute::request_counter_{0};

DocumentsRoute::DocumentsRoute()
{
    ServerLogger::logInfo("DocumentsRoute initialized");
}

DocumentsRoute::~DocumentsRoute() = default;

bool DocumentsRoute::match(const std::string& method, const std::string& path)
{
    if ((method == "POST" && path == "/add_documents") ||
        (method == "POST" && path == "/remove_documents") ||
        (method == "GET" && path == "/list_documents") ||
        (method == "POST" && path == "/info_documents") ||
        (method == "OPTIONS" && (path == "/add_documents" || path == "/remove_documents" || 
                                path == "/list_documents" || path == "/info_documents")))
    {
        current_endpoint_ = path;
        current_method_ = method;
        return true;
    }
    return false;
}

void DocumentsRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received %s request for endpoint: %s", 
                              std::this_thread::get_id(), current_method_.c_str(), current_endpoint_.c_str());

        if (current_method_ == "OPTIONS")
        {
            handleOptions(sock);
        }
        else if (current_endpoint_ == "/add_documents")
        {
            handleAddDocuments(sock, body);
        }
        else if (current_endpoint_ == "/remove_documents")
        {
            handleRemoveDocuments(sock, body);
        }
        else if (current_endpoint_ == "/list_documents")
        {
            handleListDocuments(sock);
        }
        else if (current_endpoint_ == "/info_documents")
        {
            handleDocumentsInfo(sock, body);
        }
        else
        {
            sendErrorResponse(sock, 404, "Endpoint not found");
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling documents request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleAddDocuments(SocketType sock, const std::string& body)
{
    std::string requestId; // Declare here so it's accessible in catch blocks

    try
    {
        ServerLogger::logInfo("[Thread %u] Received add documents request", std::this_thread::get_id());

        // Check for empty body
        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        // Parse JSON request
        json j;
        try
        {
            j = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Parse the request using the DTO model
        kolosal::retrieval::AddDocumentsRequest request;
        try
        {
            request.from_json(j);
        }
        catch (const std::runtime_error& ex)
        {
            sendErrorResponse(sock, 400, ex.what());
            return;
        }

        // Validate the request
        if (!request.validate())
        {
            sendErrorResponse(sock, 400, "Invalid request parameters");
            return;
        }

        // Generate unique request ID
        requestId = "doc-" + std::to_string(++request_counter_) + "-" + 
                   std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count());

        ServerLogger::logInfo("[Thread %u] Processing %zu documents for indexing (Request ID: %s)", 
                              std::this_thread::get_id(), request.documents.size(), requestId.c_str());

        // Initialize document service if needed
        if (!ensureDocumentService())
        {
            sendErrorResponse(sock, 500, "Failed to initialize document service", "service_error");
            return;
        }

        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected)
        {
            sendErrorResponse(sock, 503, "Database connection failed", "service_unavailable");
            return;
        }

        // Process documents
        ServerLogger::logDebug("[Thread %u] Submitting documents for processing", std::this_thread::get_id());
        
        auto response_future = document_service_->addDocuments(request);
        
        // Wait for processing to complete
        kolosal::retrieval::AddDocumentsResponse response = response_future.get();

        // Send successful response
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, 200, response.to_json().dump(), headers);

        ServerLogger::logInfo("[Thread %u] Successfully processed documents - Success: %d, Failed: %d", 
                              std::this_thread::get_id(), response.successful_count, response.failed_count);
    }
    catch (const json::exception& ex)
    {
        ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling add documents request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleRemoveDocuments(SocketType sock, const std::string& body)
{
    std::string requestId; // Declare here so it's accessible in catch blocks

    try
    {
        ServerLogger::logInfo("[Thread %u] Received remove documents request", std::this_thread::get_id());

        // Check for empty body
        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        // Parse JSON request
        json j;
        try
        {
            j = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Parse the request using the DTO model
        kolosal::retrieval::RemoveDocumentsRequest request;
        try
        {
            request.from_json(j);
        }
        catch (const std::runtime_error& ex)
        {
            sendErrorResponse(sock, 400, ex.what());
            return;
        }

        // Validate the request
        if (!request.validate())
        {
            sendErrorResponse(sock, 400, "Invalid request parameters: document_ids cannot be empty");
            return;
        }

        // Generate unique request ID
        requestId = "rem-" + std::to_string(++request_counter_) + "-" + 
                   std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count());

        ServerLogger::logInfo("[Thread %u] Processing removal of %zu documents (Request ID: %s)", 
                              std::this_thread::get_id(), request.ids.size(), requestId.c_str());

        // Initialize document service if needed
        if (!ensureDocumentService())
        {
            sendErrorResponse(sock, 500, "Failed to initialize document service", "service_error");
            return;
        }

        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected)
        {
            sendErrorResponse(sock, 503, "Database connection failed", "service_unavailable");
            return;
        }

        // Process removal
        ServerLogger::logDebug("[Thread %u] Submitting documents for removal", std::this_thread::get_id());
        
        auto response_future = document_service_->removeDocuments(request);
        
        // Wait for processing to complete
        kolosal::retrieval::RemoveDocumentsResponse response = response_future.get();

        // Send successful response
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, 200, response.to_json().dump(), headers);

        ServerLogger::logInfo("[Thread %u] Successfully processed document removal - Removed: %d, Failed: %d, Not Found: %d", 
                              std::this_thread::get_id(), response.removed_count, response.failed_count, response.not_found_count);
    }
    catch (const json::exception& ex)
    {
        ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling remove documents request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleListDocuments(SocketType sock)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received list documents request", std::this_thread::get_id());

        // Initialize document service if needed
        if (!ensureDocumentService())
        {
            sendErrorResponse(sock, 500, "Failed to initialize document service", "service_error");
            return;
        }

        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected)
        {
            sendErrorResponse(sock, 503, "Database connection failed", "service_unavailable");
            return;
        }

        // Get list of document IDs
        ServerLogger::logDebug("[Thread %u] Fetching document list", std::this_thread::get_id());
        
        auto list_future = document_service_->listDocuments();
        std::vector<std::string> document_ids = list_future.get();

        // Create response
        kolosal::retrieval::ListDocumentsResponse response;
        response.document_ids = std::move(document_ids);
        response.total_count = static_cast<int>(response.document_ids.size());
        response.collection_name = "documents"; // Default collection name

        // Send successful response
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, 200, response.to_json().dump(), headers);

        ServerLogger::logInfo("[Thread %u] Successfully returned list of %d documents", 
                              std::this_thread::get_id(), response.total_count);
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling list documents request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleDocumentsInfo(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received info documents request", std::this_thread::get_id());

        // Check for empty body
        if (body.empty())
        {
            sendErrorResponse(sock, 400, "Request body is empty");
            return;
        }

        // Parse JSON request
        json j;
        try
        {
            j = json::parse(body);
        }
        catch (const json::parse_error& ex)
        {
            sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
            return;
        }

        // Parse the request using the DTO model
        kolosal::retrieval::DocumentsInfoRequest request;
        try
        {
            request.from_json(j);
        }
        catch (const std::runtime_error& ex)
        {
            sendErrorResponse(sock, 400, ex.what());
            return;
        }

        // Validate the request
        if (!request.validate())
        {
            sendErrorResponse(sock, 400, "Invalid request parameters: ids cannot be empty");
            return;
        }

        ServerLogger::logInfo("[Thread %u] Processing info request for %zu documents", 
                              std::this_thread::get_id(), request.ids.size());

        // Initialize document service if needed
        if (!ensureDocumentService())
        {
            sendErrorResponse(sock, 500, "Failed to initialize document service", "service_error");
            return;
        }

        // Test connection
        bool connected = document_service_->testConnection().get();
        if (!connected)
        {
            sendErrorResponse(sock, 503, "Database connection failed", "service_unavailable");
            return;
        }

        // Get documents info
        ServerLogger::logDebug("[Thread %u] Fetching document info", std::this_thread::get_id());
        
        auto info_future = document_service_->getDocumentsInfo(request.ids);
        auto document_infos = info_future.get();

        // Create response
        kolosal::retrieval::DocumentsInfoResponse response;
        response.collection_name = "documents"; // Default collection name

        for (const auto& [id, info_opt] : document_infos)
        {
            if (info_opt.has_value())
            {
                kolosal::retrieval::DocumentInfo doc_info;
                doc_info.id = id;
                doc_info.text = info_opt.value().first;
                doc_info.metadata = info_opt.value().second;
                response.documents.push_back(std::move(doc_info));
                response.found_count++;
            }
            else
            {
                response.not_found_ids.push_back(id);
                response.not_found_count++;
            }
        }

        // Send successful response
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, 200, response.to_json().dump(), headers);

        ServerLogger::logInfo("[Thread %u] Successfully returned info for %d/%zu documents", 
                              std::this_thread::get_id(), response.found_count, request.ids.size());
    }
    catch (const json::exception& ex)
    {
        ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling info documents request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::handleOptions(SocketType sock)
{
    try
    {
        ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for CORS preflight", 
                               std::this_thread::get_id());

        std::map<std::string, std::string> headers = {
            {"Content-Type", "text/plain"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
            {"Access-Control-Max-Age", "86400"} // Cache preflight for 24 hours
        };
        
        send_response(sock, 200, "", headers);
        
        ServerLogger::logDebug("[Thread %u] Successfully handled OPTIONS request", 
                               std::this_thread::get_id());
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling OPTIONS request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void DocumentsRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                      const std::string& error_type, const std::string& param)
{
    kolosal::retrieval::DocumentsErrorResponse errorResponse;
    errorResponse.error = message;
    errorResponse.error_type = error_type;
    errorResponse.param = param;

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, errorResponse.to_json().dump(), headers);
}

bool DocumentsRoute::ensureDocumentService()
{
    std::lock_guard<std::mutex> lock(service_mutex_);
    if (!document_service_)
    {
        // Get database config from the server configuration
        auto& serverConfig = ServerConfig::getInstance();
        DatabaseConfig db_config = serverConfig.database;
        
        // Ensure Qdrant is configured with proper defaults if not set
        if (db_config.qdrant.host.empty()) {
            db_config.qdrant.host = "localhost";
        }
        if (db_config.qdrant.port == 0) {
            db_config.qdrant.port = 6333;
        }
        if (db_config.qdrant.collectionName.empty()) {
            db_config.qdrant.collectionName = "documents";
        }
        if (db_config.qdrant.defaultEmbeddingModel.empty()) {
            db_config.qdrant.defaultEmbeddingModel = "text-embedding-3-small";
        }
        if (db_config.qdrant.timeout == 0) {
            db_config.qdrant.timeout = 30;
        }
        if (db_config.qdrant.maxConnections == 0) {
            db_config.qdrant.maxConnections = 10;
        }
        if (db_config.qdrant.connectionTimeout == 0) {
            db_config.qdrant.connectionTimeout = 5;
        }
        if (db_config.qdrant.embeddingBatchSize == 0) {
            db_config.qdrant.embeddingBatchSize = 5;
        }
        
        document_service_ = std::make_unique<kolosal::retrieval::DocumentService>(db_config);
        
        // Initialize service
        bool initialized = document_service_->initialize().get();
        if (!initialized)
        {
            return false;
        }
        
        ServerLogger::logInfo("DocumentService initialized successfully");
    }
    return true;
}

} // namespace kolosal

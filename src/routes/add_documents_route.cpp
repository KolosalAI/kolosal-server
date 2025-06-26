#include "kolosal/routes/add_documents_route.hpp"
#include "kolosal/models/add_documents_model.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/completion_monitor.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <memory>

using json = nlohmann::json;

namespace kolosal
{

std::atomic<long long> AddDocumentsRoute::request_counter_{0};

AddDocumentsRoute::AddDocumentsRoute()
    : monitor_(std::make_unique<CompletionMonitor>())
{
    ServerLogger::logInfo("AddDocumentsRoute initialized");
}

AddDocumentsRoute::~AddDocumentsRoute() = default;

bool AddDocumentsRoute::match(const std::string& method, const std::string& path)
{
    return method == "POST" && path == "/add_documents";
}

void AddDocumentsRoute::handle(SocketType sock, const std::string& body)
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
        AddDocumentsRequest request;
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

        // Start monitoring
        monitor_->startRequest("document-indexing", "add_documents");        // Initialize document service if needed
        {
            std::lock_guard<std::mutex> lock(service_mutex_);
            if (!document_service_)
            {
                // Create a basic database config - in a production environment,
                // this would be passed from the main server configuration
                DatabaseConfig db_config;
                db_config.qdrant.enabled = true;
                db_config.qdrant.host = "localhost";
                db_config.qdrant.port = 6333;
                db_config.qdrant.collectionName = "documents";
                db_config.qdrant.defaultEmbeddingModel = "text-embedding-3-small";
                db_config.qdrant.timeout = 30;
                db_config.qdrant.maxConnections = 10;
                db_config.qdrant.connectionTimeout = 5;
                
                document_service_ = std::make_unique<DocumentService>(db_config);
                
                // Initialize service
                bool initialized = document_service_->initialize().get();
                if (!initialized)
                {
                    sendErrorResponse(sock, 500, "Failed to initialize document service", "service_error");
                    return;
                }
                
                ServerLogger::logInfo("DocumentService initialized successfully");
            }
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
        AddDocumentsResponse response = response_future.get();

        // Complete monitoring
        monitor_->completeRequest(requestId);

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
        // Mark request as failed if monitoring was started
        if (!requestId.empty())
        {
            monitor_->failRequest(requestId);
        }

        ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
    }
    catch (const std::exception& ex)
    {
        // Mark request as failed if monitoring was started
        if (!requestId.empty())
        {
            monitor_->failRequest(requestId);
        }

        ServerLogger::logError("[Thread %u] Error handling add documents request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void AddDocumentsRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                         const std::string& error_type, const std::string& param)
{
    AddDocumentsErrorResponse errorResponse;
    errorResponse.error = message;
    errorResponse.error_type = error_type;
    errorResponse.param = param;

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, errorResponse.to_json().dump(), headers);
}

} // namespace kolosal

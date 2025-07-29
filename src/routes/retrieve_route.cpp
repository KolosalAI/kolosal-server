#include "kolosal/routes/retrieve_route.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
// #include "kolosal/completion_monitor.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <memory>

using json = nlohmann::json;

namespace kolosal
{

std::atomic<long long> RetrieveRoute::request_counter_{0};

RetrieveRoute::RetrieveRoute()
    // : monitor_(std::make_unique<CompletionMonitor>())
{
    ServerLogger::logInfo("RetrieveRoute initialized");
}

RetrieveRoute::~RetrieveRoute() = default;

bool RetrieveRoute::match(const std::string& method, const std::string& path)
{
    if ((method == "POST" || method == "OPTIONS") && path == "/retrieve")
    {
        current_method_ = method;
        return true;
    }
    return false;
}

void RetrieveRoute::handle(SocketType sock, const std::string& body)
{
    std::string requestId; // Declare here so it's accessible in catch blocks

    try
    {
        ServerLogger::logInfo("[Thread %u] Received %s request for /retrieve", 
                              std::this_thread::get_id(), current_method_.c_str());

        // Handle OPTIONS request for CORS preflight
        if (current_method_ == "OPTIONS")
        {
            handleOptions(sock);
            return;
        }

        ServerLogger::logInfo("[Thread %u] Processing retrieve request", std::this_thread::get_id());

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
        kolosal::retrieval::RetrieveRequest request;
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
        requestId = "ret-" + std::to_string(++request_counter_) + "-" + 
                   std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count());

        ServerLogger::logInfo("[Thread %u] Processing retrieval for query: '%s' (k=%d, Request ID: %s)", 
                              std::this_thread::get_id(), request.query.c_str(), request.k, requestId.c_str());

        // Start monitoring
        // monitor_->startRequest("document-retrieval", "retrieve");

        // Initialize document service if needed
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

        // Process retrieval
        ServerLogger::logDebug("[Thread %u] Submitting retrieval for processing", std::this_thread::get_id());
        
        auto response_future = document_service_->retrieveDocuments(request);
        
        // Wait for processing to complete
        kolosal::retrieval::RetrieveResponse response = response_future.get();

        // Complete monitoring
        // monitor_->completeRequest(requestId);

        // Send successful response
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, 200, response.to_json().dump(), headers);

        ServerLogger::logInfo("[Thread %u] Successfully retrieved %d documents for query", 
                              std::this_thread::get_id(), response.total_found);
    }
    catch (const json::exception& ex)
    {
        // Mark request as failed if monitoring was started
        if (!requestId.empty())
        {
            // monitor_->failRequest(requestId);
        }

        ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
    }
    catch (const std::exception& ex)
    {
        // Mark request as failed if monitoring was started
        if (!requestId.empty())
        {
            // monitor_->failRequest(requestId);
        }

        ServerLogger::logError("[Thread %u] Error handling retrieve request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void RetrieveRoute::handleOptions(SocketType sock)
{
    try
    {
        ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for /retrieve endpoint", 
                               std::this_thread::get_id());

        std::map<std::string, std::string> headers = {
            {"Content-Type", "text/plain"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
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

void RetrieveRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                     const std::string& error_type, const std::string& param)
{
    kolosal::retrieval::RetrieveErrorResponse errorResponse;
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

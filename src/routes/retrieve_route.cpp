#include "kolosal/routes/retrieve_route.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
// #include "kolosal/completion_monitor.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <memory>

// Ensure we're building the library for this definition
#ifndef KOLOSAL_SERVER_BUILD
#define KOLOSAL_SERVER_BUILD
#endif

using json = nlohmann::json;

namespace kolosal
{

// Define the static member - using workaround for MSVC DLL export issue
// std::atomic<long long> RetrieveRoute::request_counter_{0};

// Static function to get request counter - avoids DLL export issues
static std::atomic<long long>& getRequestCounter() {
    static std::atomic<long long> counter{0};
    return counter;
}

RetrieveRoute::RetrieveRoute()
    // : monitor_(std::make_unique<CompletionMonitor>())
{
    ServerLogger::logInfo("RetrieveRoute initialized");
}

RetrieveRoute::~RetrieveRoute() = default;

bool RetrieveRoute::match(const std::string& method, const std::string& path)
{
    return method == "POST" && path == "/retrieve";
}

void RetrieveRoute::handle(SocketType sock, const std::string& body)
{
    std::string requestId; // Declare here so it's accessible in catch blocks

    try
    {
        ServerLogger::logInfo("[Thread %u] Received retrieve request", std::this_thread::get_id());

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
            std::string validation_error = "Invalid request parameters - ";
            if (request.query.empty()) {
                validation_error += "query cannot be empty";
            } else if (request.k <= 0 || request.k > 1000) {
                validation_error += "k must be between 1 and 1000 (got " + std::to_string(request.k) + ")";
            } else if (request.score_threshold < 0.0f || request.score_threshold > 1.0f) {
                validation_error += "score_threshold must be between 0.0 and 1.0 (got " + std::to_string(request.score_threshold) + ")";
            } else {
                validation_error += "check request parameters";
            }
            
            sendErrorResponse(sock, 400, validation_error, "invalid_request_error");
            return;
        }

        // Generate unique request ID
        requestId = "ret-" + std::to_string(++getRequestCounter()) + "-" + 
                   std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count());

        ServerLogger::logInfo("[Thread %u] Processing retrieval for query: '%s' (k=%d, Request ID: %s)", 
                              std::this_thread::get_id(), request.query.c_str(), request.k, requestId.c_str());

        // Start monitoring
        // monitor_->startRequest("document-retrieval", "retrieve");

        // Get document service from ServerAPI
        auto& serverAPI = ServerAPI::instance();
        
        try {
            auto& document_service = serverAPI.getDocumentService();
            
            // Test connection first
            ServerLogger::logDebug("[Thread %u] Testing database connection", std::this_thread::get_id());
            bool connected = document_service.testConnection().get();
            if (!connected)
            {
                ServerLogger::logError("[Thread %u] Database connection test failed", std::this_thread::get_id());
                sendErrorResponse(sock, 503, "Database connection failed - ensure Qdrant is running and accessible", "service_unavailable");
                return;
            }
            
            ServerLogger::logDebug("[Thread %u] Database connection successful", std::this_thread::get_id());

            // Process retrieval
            ServerLogger::logDebug("[Thread %u] Submitting retrieval for processing", std::this_thread::get_id());
            
            auto response_future = document_service.retrieveDocuments(request);
        
            // Wait for processing to complete
            kolosal::retrieval::RetrieveResponse response = response_future.get();

            // Complete monitoring
            // monitor_->completeRequest(requestId);

            // Check if we got any results and provide helpful information
            if (response.total_found == 0)
            {
                ServerLogger::logInfo("[Thread %u] No documents found for query '%s' - this could be normal if no matching documents exist", 
                                    std::this_thread::get_id(), request.query.c_str());
            }

            // Send successful response (even if no documents found)
            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"},
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "POST, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
            };
            send_response(sock, 200, response.to_json().dump(), headers);

            ServerLogger::logInfo("[Thread %u] Successfully processed retrieval request - returned %d documents", 
                                  std::this_thread::get_id(), response.total_found);
                                  
        } catch (const std::runtime_error& ex) {
            // Handle DocumentService specific errors with more context
            std::string error_msg = ex.what();
            ServerLogger::logError("[Thread %u] DocumentService error: %s", std::this_thread::get_id(), error_msg.c_str());
            
            // Provide specific error codes and helpful messages
            if (error_msg.find("not initialized") != std::string::npos) {
                sendErrorResponse(sock, 500, "Document service not initialized - server may be starting up. Check GET /health for status", "service_not_ready");
            } else if (error_msg.find("Qdrant is disabled") != std::string::npos) {
                sendErrorResponse(sock, 503, "Document retrieval is disabled in server configuration. Check GET /health for details", "service_disabled");
            } else if (error_msg.find("embedding") != std::string::npos) {
                sendErrorResponse(sock, 500, "Embedding generation failed - " + error_msg + ". Check GET /health for service status", "embedding_error");
            } else if (error_msg.find("Vector search failed") != std::string::npos) {
                sendErrorResponse(sock, 503, "Vector search failed - " + error_msg + ". Check GET /health for database status", "search_error");
            } else if (error_msg.find("Collection") != std::string::npos && error_msg.find("does not exist") != std::string::npos) {
                sendErrorResponse(sock, 404, "No documents collection found - please index some documents first. Check GET /health for collection status", "collection_not_found");
            } else {
                sendErrorResponse(sock, 500, "DocumentService error: " + error_msg + ". Check GET /health for detailed diagnostics", "service_error");
            }
            return;
        }
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

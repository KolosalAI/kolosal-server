#include "kolosal/routes/retrieval/retrieve_route.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace kolosal
{

std::atomic<long long> RetrieveRoute::request_counter_{0};

RetrieveRoute::RetrieveRoute()
{
    ServerLogger::logInfo("RetrieveRoute initialized");
}

RetrieveRoute::~RetrieveRoute() = default;

bool RetrieveRoute::match(const std::string& method, const std::string& path)
{
    if ((method == "POST" && path == "/retrieve") ||
        (method == "OPTIONS" && path == "/retrieve"))
    {
        current_endpoint_ = path;
        current_method_ = method;
        return true;
    }
    return false;
}

void RetrieveRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received %s request for endpoint: %s", 
                              std::this_thread::get_id(), current_method_.c_str(), current_endpoint_.c_str());

        if (current_method_ == "OPTIONS")
        {
            handleOptions(sock);
        }
        else if (current_endpoint_ == "/retrieve")
        {
            handleRetrieve(sock, body);
        }
        else
        {
            sendErrorResponse(sock, 404, "Endpoint not found");
        }
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling retrieve request: %s", 
                               std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void RetrieveRoute::handleRetrieve(SocketType sock, const std::string& body)
{
    std::string requestId;

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
            sendErrorResponse(sock, 400, "Invalid request parameters");
            return;
        }

        // Generate unique request ID
        requestId = "ret-" + std::to_string(++request_counter_) + "-" + 
                   std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count());

        ServerLogger::logInfo("[Thread %u] Processing retrieval for query: '%s' (k=%d, Request ID: %s)", 
                              std::this_thread::get_id(), request.query.c_str(), request.k, requestId.c_str());

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

        // Process retrieval
        ServerLogger::logDebug("[Thread %u] Submitting retrieval for processing", std::this_thread::get_id());
        
        auto response_future = document_service_->retrieveDocuments(request);
        
        // Wait for processing to complete
        kolosal::retrieval::RetrieveResponse response = response_future.get();

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
        ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling retrieve request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void RetrieveRoute::handleOptions(SocketType sock)
{
    try
    {
        ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for CORS preflight", 
                               std::this_thread::get_id());

        std::map<std::string, std::string> headers = {
            {"Content-Type", "text/plain"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
            {"Access-Control-Max-Age", "86400"}
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
    json errorResponse = {
        {"error", message},
        {"error_type", error_type},
        {"param", param}
    };

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, errorResponse.dump(), headers);
}

bool RetrieveRoute::ensureDocumentService()
{
    std::lock_guard<std::mutex> lock(service_mutex_);
    if (!document_service_)
    {
        try
        {
            // Get DocumentService from ServerAPI
            document_service_ = std::make_unique<kolosal::retrieval::DocumentService>(
                kolosal::ServerAPI::instance().getDocumentService());
            ServerLogger::logInfo("RetrieveRoute: DocumentService initialized successfully");
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("RetrieveRoute: Failed to initialize DocumentService: %s", ex.what());
            return false;
        }
    }
    return true;
}

} // namespace kolosal

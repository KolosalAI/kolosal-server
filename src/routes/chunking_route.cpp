#include "kolosal/routes/chunking_route.hpp"
#include "kolosal/models/chunking_request_model.hpp"
#include "kolosal/models/chunking_response_model.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/node_manager.h"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <algorithm>

using json = nlohmann::json;

namespace kolosal
{

ChunkingRoute::ChunkingRoute()
    : chunking_service_(std::make_unique<retrieval::ChunkingService>())
    , request_counter_(0)
{
    ServerLogger::logInfo("ChunkingRoute initialized");
}

ChunkingRoute::~ChunkingRoute() = default;

bool ChunkingRoute::match(const std::string& method, const std::string& path)
{
    if ((method == "POST" || method == "OPTIONS") && path == "/chunking")
    {
        current_method_ = method;
        return true;
    }
    return false;
}

void ChunkingRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received %s request for /chunking", 
                              std::this_thread::get_id(), current_method_.c_str());

        // Handle OPTIONS request for CORS preflight
        if (current_method_ == "OPTIONS")
        {
            handleOptions(sock);
            return;
        }

        std::string requestId;
        auto start_time = std::chrono::high_resolution_clock::now();

        ServerLogger::logInfo("[Thread %u] Processing chunking request", std::this_thread::get_id());

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
        ChunkingRequest request;
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

        // Validate the model
        if (!validateChunkingModel(request.model_name))
        {
            sendErrorResponse(sock, 404, "Model '" + request.model_name + "' not found or could not be loaded", "model_not_found", "model_name");
            return;
        }

        // Generate unique request ID
        requestId = "chunk-" + std::to_string(++request_counter_) + "-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        ServerLogger::logInfo("[Thread %u] Processing chunking request '%s' for model '%s' using method '%s'",
                              std::this_thread::get_id(), requestId.c_str(), request.model_name.c_str(), request.method.c_str());

        // Process the request based on the method
        std::future<std::vector<std::string>> chunks_future;

        if (request.method == "regular")
        {
            chunks_future = processRegularChunking(
                request.text,
                request.model_name,
                request.chunk_size,
                request.overlap
            );
        }
        else if (request.method == "semantic")
        {
            chunks_future = processSemanticChunking(
                request.text,
                request.model_name,
                request.chunk_size,
                request.overlap,
                request.max_chunk_size,
                request.similarity_threshold
            );
        }
        else
        {
            sendErrorResponse(sock, 400, "Invalid method: must be 'regular' or 'semantic'", "invalid_parameter", "method");
            return;
        }

        // Wait for processing to complete
        std::vector<std::string> chunks;
        try
        {
            chunks = chunks_future.get();
        }
        catch (const std::exception& ex)
        {
            sendErrorResponse(sock, 500, "Failed to process chunking request: " + std::string(ex.what()), "processing_error");
            return;
        }

        // Calculate processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        float processing_time_ms = static_cast<float>(duration.count());

        // Build response
        ChunkingResponse response;
        response.model_name = request.model_name;
        response.method = request.method;

        int original_tokens = estimateTokenCount(request.text);
        int total_chunk_tokens = 0;

        for (size_t i = 0; i < chunks.size(); ++i)
        {
            int chunk_tokens = estimateTokenCount(chunks[i]);
            total_chunk_tokens += chunk_tokens;
            
            ChunkData chunk_data(chunks[i], static_cast<int>(i), chunk_tokens);
            response.addChunk(chunk_data);
        }

        response.setUsage(original_tokens, total_chunk_tokens, processing_time_ms);

        // Send successful response
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, 200, response.to_json().dump(), headers);

        ServerLogger::logInfo("[Thread %u] Successfully processed chunking request '%s': %zu chunks generated in %.2fms",
                              std::this_thread::get_id(), requestId.c_str(), chunks.size(), processing_time_ms);
    }
    catch (const json::exception& ex)
    {
        ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 400, "Invalid JSON: " + std::string(ex.what()));
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling chunking request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

std::future<std::vector<std::string>> ChunkingRoute::processRegularChunking(
    const std::string& text,
    const std::string& model_name,
    int chunk_size,
    int overlap
)
{
    return std::async(std::launch::async, [=]() -> std::vector<std::string> {
        try
        {
            std::lock_guard<std::mutex> lock(service_mutex_);
            
            // Tokenize the text
            auto tokens_future = chunking_service_->tokenizeText(text, model_name);
            auto tokens = tokens_future.get();
            
            // Generate base chunks
            return chunking_service_->generateBaseChunks(text, tokens, chunk_size, overlap);
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("[Thread %u] Error in regular chunking: %s", 
                                   std::this_thread::get_id(), ex.what());
            throw;
        }
    });
}

std::future<std::vector<std::string>> ChunkingRoute::processSemanticChunking(
    const std::string& text,
    const std::string& model_name,
    int chunk_size,
    int overlap,
    int max_chunk_size,
    float similarity_threshold
)
{
    return std::async(std::launch::async, [=]() -> std::vector<std::string> {
        try
        {
            std::lock_guard<std::mutex> lock(service_mutex_);
            
            // Use the semantic chunking service
            auto chunks_future = chunking_service_->semanticChunk(
                text, model_name, chunk_size, overlap, max_chunk_size, similarity_threshold
            );
            
            return chunks_future.get();
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("[Thread %u] Error in semantic chunking: %s", 
                                   std::this_thread::get_id(), ex.what());
            throw;
        }
    });
}

void ChunkingRoute::handleOptions(SocketType sock)
{
    try
    {
        ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for /chunking endpoint", 
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

void ChunkingRoute::sendErrorResponse(
    SocketType sock,
    int status_code,
    const std::string& error_message,
    const std::string& error_type,
    const std::string& param
)
{
    json errorResponse;
    errorResponse["error"]["message"] = error_message;
    errorResponse["error"]["type"] = error_type;
    errorResponse["error"]["code"] = "";
    
    if (!param.empty())
    {
        errorResponse["error"]["param"] = param;
    }

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status_code, errorResponse.dump(), headers);
    
    ServerLogger::logError("[Thread %u] Chunking request error (%d): %s", 
                           std::this_thread::get_id(), status_code, error_message.c_str());
}

int ChunkingRoute::estimateTokenCount(const std::string& text) const
{
    // Simple estimation: roughly 4 characters per token for English text
    return std::max(1, static_cast<int>(text.length() / 4));
}

bool ChunkingRoute::validateChunkingModel(const std::string& model_name) const
{
    // Check if the model is loaded and available
    auto& nodeManager = ServerAPI::instance().getNodeManager();
    auto engine = nodeManager.getEngine(model_name);
    return engine != nullptr;
}

} // namespace kolosal

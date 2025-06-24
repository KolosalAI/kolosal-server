#include "kolosal/routes/embedding_route.hpp"
#include "kolosal/models/embedding_request_model.hpp"
#include "kolosal/models/embedding_response_model.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/completion_monitor.hpp"
#include "inference_interface.h"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <algorithm>
#include <regex>

using json = nlohmann::json;

namespace kolosal
{

EmbeddingRoute::EmbeddingRoute() 
    : monitor_(&CompletionMonitor::getInstance())
    , request_counter_(0)
{
    ServerLogger::logInfo("EmbeddingRoute initialized with completion monitoring");
}

EmbeddingRoute::~EmbeddingRoute() = default;

bool EmbeddingRoute::match(const std::string& method, const std::string& path)
{
    return (method == "POST" && (path == "/v1/embeddings" || path == "/embeddings"));
}

void EmbeddingRoute::handle(SocketType sock, const std::string& body)
{
    std::string requestId; // Declare here so it's accessible in catch blocks

    try
    {
        ServerLogger::logInfo("[Thread %u] Received embedding request", std::this_thread::get_id());

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
        EmbeddingRequest request;
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

        // Get the NodeManager and inference engine
        auto& nodeManager = ServerAPI::instance().getNodeManager();
        auto engine = nodeManager.getEngine(request.model);

        if (!engine)
        {
            sendErrorResponse(sock, 404, "Model '" + request.model + "' not found or could not be loaded", "model_not_found", "model");
            return;
        }

        // Generate unique request ID
        requestId = "emb-" + std::to_string(++request_counter_) + "-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

        // Get input texts
        std::vector<std::string> inputTexts = request.getInputTexts();
        
        ServerLogger::logInfo("[Thread %u] Processing %zu embedding request(s) for model '%s'", 
                              std::this_thread::get_id(), inputTexts.size(), request.model.c_str());

        // Start monitoring
        monitor_->startRequest(request.model, "embedding");

        // Estimate total prompt tokens
        int totalPromptTokens = 0;
        for (const auto& text : inputTexts)
        {
            totalPromptTokens += estimateTokenCount(text);
        }
        monitor_->recordInputTokens(requestId, totalPromptTokens);

        // Process embeddings
        std::vector<std::future<std::vector<float>>> embeddingFutures;
        
        if (request.hasMultipleInputs())
        {
            // Process batch request
            embeddingFutures = processEmbeddingsBatch(inputTexts, request.model, requestId);
        }
        else
        {
            // Process single request
            embeddingFutures.push_back(processEmbeddingAsync(inputTexts[0], request.model, requestId));
        }

        // Wait for all embeddings to complete and collect results
        EmbeddingResponse response;
        response.model = request.model;

        for (size_t i = 0; i < embeddingFutures.size(); ++i)
        {
            try
            {
                auto embedding = embeddingFutures[i].get(); // This will block until the future is ready
                response.addEmbedding(embedding, static_cast<int>(i));
            }
            catch (const std::exception& ex)
            {
                monitor_->failRequest(requestId);
                sendErrorResponse(sock, 500, "Failed to generate embedding for input " + std::to_string(i) + ": " + ex.what(), "server_error");
                return;
            }
        }

        // Set usage statistics
        response.setUsage(totalPromptTokens);

        // Complete monitoring
        monitor_->completeRequest(requestId);

        // Send successful response
        send_response(sock, 200, response.to_json().dump());

        ServerLogger::logInfo("[Thread %u] Successfully generated %zu embedding(s) for model '%s'", 
                              std::this_thread::get_id(), response.data.size(), request.model.c_str());
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

        ServerLogger::logError("[Thread %u] Error handling embedding request: %s", std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

std::future<std::vector<float>> EmbeddingRoute::processEmbeddingAsync(
    const std::string& input_text, 
    const std::string& model,
    const std::string& request_id)
{
    return std::async(std::launch::async, [this, input_text, model, request_id]() -> std::vector<float> {
        try
        {
            // Get the inference engine
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            auto engine = nodeManager.getEngine(model);

            if (!engine)
            {
                throw std::runtime_error("Model '" + model + "' not found or could not be loaded");
            }            // Create embedding parameters
            EmbeddingParameters params;
            params.input = input_text;
            params.normalize = true; // Default normalization for OpenAI compatibility
            
            // Set sequence ID for potential caching
            params.seqId = static_cast<int>(std::hash<std::string>{}(input_text + model) % 10000);

            if (!params.isValid())
            {
                throw std::runtime_error("Invalid embedding parameters");
            }

            // Submit embedding job
            int jobId = engine->submitEmbeddingJob(params);
            if (jobId < 0)
            {
                throw std::runtime_error("Failed to submit embedding job to inference engine");
            }

            ServerLogger::logDebug("[Thread %u] Submitted embedding job %d for model '%s'", 
                                   std::this_thread::get_id(), jobId, model.c_str());

            // Wait for job completion
            engine->waitForJob(jobId);

            // Check for errors
            if (engine->hasJobError(jobId))
            {
                std::string error = engine->getJobError(jobId);
                throw std::runtime_error("Inference error: " + error);
            }

            // Get the embedding result
            EmbeddingResult result = engine->getEmbeddingResult(jobId);

            ServerLogger::logDebug("[Thread %u] Completed embedding job %d: %zu dimensions", 
                                   std::this_thread::get_id(), jobId, result.embedding.size());

            return result.embedding;
        }
        catch (const std::exception& ex)
        {
            ServerLogger::logError("[Thread %u] Error in async embedding processing: %s", 
                                   std::this_thread::get_id(), ex.what());
            throw; // Re-throw to be handled by the caller
        }
    });
}

std::vector<std::future<std::vector<float>>> EmbeddingRoute::processEmbeddingsBatch(
    const std::vector<std::string>& input_texts,
    const std::string& model,
    const std::string& request_id)
{
    std::vector<std::future<std::vector<float>>> futures;
    futures.reserve(input_texts.size());

    // Process each text asynchronously
    for (size_t i = 0; i < input_texts.size(); ++i)
    {
        std::string batchRequestId = request_id + "-batch-" + std::to_string(i);
        futures.push_back(processEmbeddingAsync(input_texts[i], model, batchRequestId));
    }

    return futures;
}

int EmbeddingRoute::estimateTokenCount(const std::string& text) const
{
    // Simple estimation: roughly 4 characters per token for English text
    // This is a rough approximation - actual tokenization would be more accurate
    return (std::max)(1, static_cast<int>(text.length() / 4));
}

bool EmbeddingRoute::validateEmbeddingModel(const std::string& model) const
{
    // For now, we assume any loaded model can generate embeddings
    // In a more sophisticated implementation, we could check model capabilities
    auto& nodeManager = ServerAPI::instance().getNodeManager();
    auto engine = nodeManager.getEngine(model);
    return engine != nullptr;
}

void EmbeddingRoute::sendErrorResponse(
    SocketType sock, 
    int status_code, 
    const std::string& error_message,
    const std::string& error_type,
    const std::string& param)
{
    EmbeddingErrorResponse errorResponse;
    errorResponse.error.message = error_message;
    errorResponse.error.type = error_type;
    errorResponse.error.param = param;
    errorResponse.error.code = "";

    send_response(sock, status_code, errorResponse.to_json().dump());
    
    ServerLogger::logError("[Thread %u] Embedding request error (%d): %s", 
                           std::this_thread::get_id(), status_code, error_message.c_str());
}

} // namespace kolosal

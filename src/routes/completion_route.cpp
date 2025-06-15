#pragma once

#include "kolosal/routes/completion_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/models/completion_request_model.hpp"
#include "kolosal/models/completion_response_model.hpp"
#include "kolosal/models/completion_response_chunk_model.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/node_manager.h"
#include "inference_interface.h"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <memory>
#include <variant>

using json = nlohmann::json;

namespace kolosal
{
    namespace
    {
        /**
         * @brief Builds CompletionParameters from a CompletionRequest
         * Following the ModelManager pattern from the example
         */
        CompletionParameters buildCompletionParameters(const CompletionRequest &request)
        {
            CompletionParameters params;

            // Set prompt based on request format
            if (std::holds_alternative<std::string>(request.prompt))
            {
                params.prompt = std::get<std::string>(request.prompt);
            }
            else if (std::holds_alternative<std::vector<std::string>>(request.prompt))
            {
                // Join multiple prompts with newlines if array is provided
                const auto &prompts = std::get<std::vector<std::string>>(request.prompt);
                std::ostringstream joined;
                for (size_t i = 0; i < prompts.size(); ++i)
                {
                    joined << prompts[i];
                    if (i < prompts.size() - 1)
                    {
                        joined << "\n";
                    }
                }
                params.prompt = joined.str();
            }

            // Map parameters from request to our format
            if (request.seed.has_value())
            {
                params.randomSeed = request.seed.value();
            }

            if (request.max_tokens.has_value())
            {
                params.maxNewTokens = request.max_tokens.value();
            }
            else
            {
                // Use a reasonable default if not specified (OpenAI default is 16)
                params.maxNewTokens = 16;
            }

            // Copy other parameters
            params.temperature = static_cast<float>(request.temperature);
            params.topP = static_cast<float>(request.top_p);
            params.streaming = request.stream;

            // Set unique sequence ID based on timestamp
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            static int seqCounter = 0;
            params.seqId = static_cast<int>(timestamp * 1000 + seqCounter++);

            return params;
        }

        /**
         * @brief Updates usage statistics for completion response
         */
        void updateUsageStats(CompletionResponse &response, const CompletionResult &result, int promptTokens)
        {
            response.usage.prompt_tokens = promptTokens;
            response.usage.completion_tokens = static_cast<int>(result.tokens.size());
            response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
        }

        /**
         * @brief Estimates prompt token count (simple approximation)
         */
        int estimatePromptTokens(const std::string &prompt)
        {
            return prompt.length() / 4; // Rough approximation: 4 chars per token
        }
    }

    bool CompletionsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && (path == "/v1/completions" || path == "/completions"));
    }
    
    void CompletionsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Check for empty body
            if (body.empty())
            {
                throw std::invalid_argument("Request body is empty");
            }

            auto j = json::parse(body);
            ServerLogger::logInfo("[Thread %u] Received completion request", std::this_thread::get_id());

            // Parse the request
            CompletionRequest request;
            request.from_json(j);

            if (!request.validate())
            {
                throw std::invalid_argument("Invalid request parameters");
            }

            // Get the NodeManager and inference engine
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            auto engine = nodeManager.getEngine(request.model);

            if (!engine)
            {
                throw std::runtime_error("Model '" + request.model + "' not found or could not be loaded");
            }

            // Build inference parameters following ModelManager pattern
            CompletionParameters inferenceParams = buildCompletionParameters(request);

            // Estimate prompt tokens for usage tracking
            int estimatedPromptTokens = estimatePromptTokens(inferenceParams.prompt);

            if (request.stream)
            {
                // Handle streaming response
                ServerLogger::logInfo("[Thread %u] Processing streaming completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit completion job to inference engine");
                }

                // Create a persistent ID for this completion
                std::string completionId = "cmpl-" + std::to_string(std::time(nullptr)) + "-" + std::to_string(jobId);

                // Start the streaming response with proper SSE headers
                begin_streaming_response(sock, 200, {{"Content-Type", "text/event-stream"}, {"Cache-Control", "no-cache"}});

                std::string previousText = "";

                // Poll for results until job is finished
                while (!engine->isJobFinished(jobId))
                {
                    // Check for errors
                    if (engine->hasJobError(jobId))
                    {
                        std::string error = engine->getJobError(jobId);
                        ServerLogger::logError("[Thread %u] Inference job error: %s", std::this_thread::get_id(), error.c_str());

                        // Send error as final chunk
                        CompletionChunk errorChunk;
                        errorChunk.id = completionId;
                        errorChunk.model = request.model;

                        CompletionChunkChoice choice;
                        choice.index = 0;
                        choice.text = "";
                        choice.finish_reason = "error";
                        errorChunk.choices.push_back(choice);

                        std::string sseData = "data: " + errorChunk.to_json().dump() + "\n\n";
                        send_stream_chunk(sock, StreamChunk(sseData, false));
                        break;
                    }

                    // Get current result
                    CompletionResult result = engine->getJobResult(jobId);

                    // Check if we have new content to stream
                    if (result.text.length() > previousText.length())
                    {
                        std::string newContent = result.text.substr(previousText.length());

                        CompletionChunk chunk;
                        chunk.id = completionId;
                        chunk.model = request.model;

                        CompletionChunkChoice choice;
                        choice.index = 0;
                        choice.text = newContent;
                        choice.finish_reason = "";

                        chunk.choices.push_back(choice);

                        // Format as SSE data message
                        std::string sseData = "data: " + chunk.to_json().dump() + "\n\n";
                        send_stream_chunk(sock, StreamChunk(sseData, false));

                        previousText = result.text;
                    }

                    // Brief sleep to avoid busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                // Send final chunk with finish reason
                if (!engine->hasJobError(jobId))
                {
                    CompletionChunk finalChunk;
                    finalChunk.id = completionId;
                    finalChunk.model = request.model;

                    CompletionChunkChoice choice;
                    choice.index = 0;
                    choice.text = "";
                    choice.finish_reason = "stop";

                    finalChunk.choices.push_back(choice);

                    std::string sseData = "data: " + finalChunk.to_json().dump() + "\n\n";
                    send_stream_chunk(sock, StreamChunk(sseData, false));
                }

                // Send the final [DONE] marker required by OpenAI client
                send_stream_chunk(sock, StreamChunk("data: [DONE]\n\n", false));

                // Then terminate the stream
                send_stream_chunk(sock, StreamChunk("", true));

                ServerLogger::logInfo("[Thread %u] Completed streaming response for job %d",
                                      std::this_thread::get_id(), jobId);
            }
            else
            {
                // Handle normal (non-streaming) response
                ServerLogger::logInfo("[Thread %u] Processing non-streaming completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit completion job to inference engine");
                }

                // Wait for job completion
                engine->waitForJob(jobId);

                // Check for errors
                if (engine->hasJobError(jobId))
                {
                    std::string error = engine->getJobError(jobId);
                    throw std::runtime_error("Inference error: " + error);
                }

                // Get the final result
                CompletionResult result = engine->getJobResult(jobId);

                // Build response
                CompletionResponse response;
                response.model = request.model;

                CompletionChoice choice;
                choice.index = 0;
                choice.text = result.text;
                choice.finish_reason = "stop";

                response.choices.push_back(choice);

                // Update usage statistics
                updateUsageStats(response, result, estimatedPromptTokens);

                // Send the response
                send_response(sock, 200, response.to_json().dump());

                ServerLogger::logInfo("[Thread %u] Completed non-streaming response for job %d (%.2f tokens/sec)",
                                      std::this_thread::get_id(), jobId, result.tps);
            }
        }
        catch (const json::exception &ex)
        {
            // Specifically handle JSON parsing errors
            ServerLogger::logError("[Thread %u] JSON parsing error: %s",
                                   std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Invalid JSON: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling completion: %s",
                                   std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Error: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 400, jError.dump());
        }
    }

} // namespace kolosal
#pragma once

#include "kolosal/routes/chat_completion_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/models/chat_response_model.hpp"
#include "kolosal/models/chat_response_chunk_model.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/completion_monitor.hpp"
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

using json = nlohmann::json;

namespace kolosal
{
    namespace
    {
        /**
         * @brief Builds ChatCompletionParameters from a ChatCompletionRequest
         * Following the ModelManager pattern from the example
         */
        ChatCompletionParameters buildChatCompletionParameters(const ChatCompletionRequest &request)
        {
            ChatCompletionParameters params;

            // Convert messages
            params.messages.clear();
            for (const auto &msg : request.messages)
            {
                params.messages.emplace_back(msg.role, msg.content);
            }

            // Set generation parameters
            params.temperature = static_cast<float>(request.temperature);
            params.topP = static_cast<float>(request.top_p);
            params.streaming = request.stream;

            // Set max tokens if specified
            if (request.max_tokens.has_value())
            {
                params.maxNewTokens = request.max_tokens.value();
            }
            else
            {
                params.maxNewTokens = 128; // Default value
            }

            // Set seed if specified
            if (request.seed.has_value())
            {
                params.randomSeed = request.seed.value();
            }

            return params;
        }

        /**
         * @brief Converts tokens per second and token count to usage statistics
         */
        void updateUsageStats(ChatCompletionResponse &response, const CompletionResult &result, int promptTokens)
        {
            response.usage.prompt_tokens = promptTokens;
            response.usage.completion_tokens = static_cast<int>(result.tokens.size());
            response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
        }

        /**
         * @brief Estimates prompt token count (simple approximation)
         */
        int estimatePromptTokens(const std::vector<ChatMessage> &messages)
        {
            int totalChars = 0;
            for (const auto &msg : messages)
            {
                totalChars += msg.content.length() + msg.role.length() + 10; // +10 for formatting
            }
            return totalChars / 4; // Rough approximation: 4 chars per token
        }
    }

    ChatCompletionsRoute::ChatCompletionsRoute() : monitor_(&CompletionMonitor::getInstance())
    {
        ServerLogger::logInfo("ChatCompletionsRoute initialized with completion monitoring");
    }

    ChatCompletionsRoute::~ChatCompletionsRoute() = default;

    bool ChatCompletionsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && (path == "/v1/chat/completions" || path == "/chat/completions"));
    }
    void ChatCompletionsRoute::handle(SocketType sock, const std::string &body)
    {
        std::string requestId; // Declare here so it's accessible in catch blocks

        try
        {
            // Check for empty body
            if (body.empty())
            {
                throw std::invalid_argument("Request body is empty");
            }

            auto j = json::parse(body);
            ServerLogger::logInfo("[Thread %u] Received chat completion request", std::this_thread::get_id());

            // Parse the request
            ChatCompletionRequest request;
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
            ChatCompletionParameters inferenceParams = buildChatCompletionParameters(request);

            // Estimate prompt tokens for usage tracking
            int estimatedPromptTokens = estimatePromptTokens(request.messages);

            // Start completion monitoring
            requestId = monitor_->startRequest(request.model, "default");
            monitor_->recordInputTokens(requestId, estimatedPromptTokens);

            if (request.stream)
            {
                // Handle streaming response
                ServerLogger::logInfo("[Thread %u] Processing streaming chat completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitChatCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit chat completion job to inference engine");
                }

                // Create a persistent ID for this completion
                std::string completionId = "chatcmpl-" + std::to_string(std::time(nullptr)) + "-" + std::to_string(jobId); // Start the streaming response with proper SSE headers
                begin_streaming_response(sock, 200, {{"Content-Type", "text/event-stream"}, {"Cache-Control", "no-cache"}});

                bool sentFirstChunk = false;
                bool firstTokenRecorded = false;
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
                        ChatCompletionChunk errorChunk;
                        errorChunk.id = completionId;
                        errorChunk.model = request.model;

                        ChatCompletionChunkChoice choice;
                        choice.index = 0;
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
                        // Record first token if this is the first output
                        if (!firstTokenRecorded && result.text.length() > 0)
                        {
                            monitor_->recordFirstToken(requestId);
                            firstTokenRecorded = true;
                        }

                        std::string newContent = result.text.substr(previousText.length());

                        // Record output tokens (approximate by character count)
                        int newTokens = static_cast<int>(newContent.length() / 4); // Rough approximation
                        for (int i = 0; i < newTokens; ++i)
                        {
                            monitor_->recordOutputToken(requestId);
                        }

                        ChatCompletionChunk chunk;
                        chunk.id = completionId;
                        chunk.model = request.model;

                        ChatCompletionChunkChoice choice;
                        choice.index = 0;

                        if (!sentFirstChunk)
                        {
                            // First chunk should include role
                            choice.delta.role = "assistant";
                            choice.delta.content = newContent;
                            choice.finish_reason = "";
                            sentFirstChunk = true;
                        }
                        else
                        {
                            choice.delta.content = newContent;
                            choice.finish_reason = "";
                        }

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
                    ChatCompletionChunk finalChunk;
                    finalChunk.id = completionId;
                    finalChunk.model = request.model;

                    ChatCompletionChunkChoice choice;
                    choice.index = 0;
                    choice.delta.content = "";
                    choice.finish_reason = "stop";

                    finalChunk.choices.push_back(choice);

                    std::string sseData = "data: " + finalChunk.to_json().dump() + "\n\n";
                    send_stream_chunk(sock, StreamChunk(sseData, false));
                } // Send the final [DONE] marker required by OpenAI client
                send_stream_chunk(sock, StreamChunk("data: [DONE]\n\n", false));

                // Then terminate the stream
                send_stream_chunk(sock, StreamChunk("", true));

                // Complete the monitoring for streaming request
                if (engine->hasJobError(jobId))
                {
                    monitor_->failRequest(requestId);
                }
                else
                {
                    monitor_->completeRequest(requestId);
                }

                ServerLogger::logInfo("[Thread %u] Completed streaming response for job %d",
                                      std::this_thread::get_id(), jobId);
            }
            else
            {
                // Handle normal (non-streaming) response
                ServerLogger::logInfo("[Thread %u] Processing non-streaming chat completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitChatCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit chat completion job to inference engine");
                } // Wait for job completion
                engine->waitForJob(jobId);

                // Check for errors
                if (engine->hasJobError(jobId))
                {
                    monitor_->failRequest(requestId);
                    std::string error = engine->getJobError(jobId);
                    throw std::runtime_error("Inference error: " + error);
                }

                // Get the final result
                CompletionResult result = engine->getJobResult(jobId);

                // Record first token and output tokens for non-streaming
                if (result.text.length() > 0)
                {
                    monitor_->recordFirstToken(requestId);

                    // Record output tokens (approximate by result tokens size)
                    int outputTokens = static_cast<int>(result.tokens.size());
                    for (int i = 0; i < outputTokens; ++i)
                    {
                        monitor_->recordOutputToken(requestId);
                    }
                }

                // Build response
                ChatCompletionResponse response;

                ChatCompletionChoice choice;
                choice.index = 0;
                choice.message.role = "assistant";
                choice.message.content = result.text;
                choice.finish_reason = "stop";

                response.choices.push_back(choice);

                // Update usage statistics
                updateUsageStats(response, result, estimatedPromptTokens);

                // Complete the monitoring
                monitor_->completeRequest(requestId);

                // Send the response
                send_response(sock, 200, response.to_json().dump());

                ServerLogger::logInfo("[Thread %u] Completed non-streaming response for job %d (%.2f tokens/sec)",
                                      std::this_thread::get_id(), jobId, result.tps);
            }
        }
        catch (const json::exception &ex)
        {
            // Mark request as failed if monitoring was started
            if (!requestId.empty())
            {
                monitor_->failRequest(requestId);
            }

            // Specifically handle JSON parsing errors
            ServerLogger::logError("[Thread %u] JSON parsing error: %s",
                                   std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Invalid JSON: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception &ex)
        {
            // Mark request as failed if monitoring was started
            if (!requestId.empty())
            {
                monitor_->failRequest(requestId);
            }

            ServerLogger::logError("[Thread %u] Error handling chat completion: %s",
                                   std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Error: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 400, jError.dump());
        }
    }

} // namespace kolosal
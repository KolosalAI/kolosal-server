#pragma once

#include "kolosal/routes/inference_chat_completion_route.hpp"
#include "kolosal/utils.hpp"
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
         * @brief Parses ChatCompletionParameters from JSON request
         */
        ChatCompletionParameters parseChatCompletionParameters(const json& j)
        {
            ChatCompletionParameters params;
            
            // Required field: messages
            if (j.contains("messages") && j["messages"].is_array()) {
                for (const auto& msgJson : j["messages"]) {
                    if (msgJson.contains("role") && msgJson.contains("content") &&
                        msgJson["role"].is_string() && msgJson["content"].is_string()) {
                        
                        Message msg(msgJson["role"].get<std::string>(), 
                                   msgJson["content"].get<std::string>());
                        params.messages.push_back(msg);
                    } else {
                        throw std::invalid_argument("Invalid message format in messages array");
                    }
                }
            } else {
                throw std::invalid_argument("Missing or invalid 'messages' field");
            }
            
            // Optional fields with defaults
            if (j.contains("randomSeed") && j["randomSeed"].is_number_integer()) {
                params.randomSeed = j["randomSeed"].get<int>();
            }
            
            if (j.contains("maxNewTokens") && j["maxNewTokens"].is_number_integer()) {
                params.maxNewTokens = j["maxNewTokens"].get<int>();
            }
            
            if (j.contains("minLength") && j["minLength"].is_number_integer()) {
                params.minLength = j["minLength"].get<int>();
            }
            
            if (j.contains("temperature") && j["temperature"].is_number()) {
                params.temperature = j["temperature"].get<float>();
            }
            
            if (j.contains("topP") && j["topP"].is_number()) {
                params.topP = j["topP"].get<float>();
            }
            
            if (j.contains("streaming") && j["streaming"].is_boolean()) {
                params.streaming = j["streaming"].get<bool>();
            }
            
            if (j.contains("kvCacheFilePath") && j["kvCacheFilePath"].is_string()) {
                params.kvCacheFilePath = j["kvCacheFilePath"].get<std::string>();
            }
            
            if (j.contains("seqId") && j["seqId"].is_number_integer()) {
                params.seqId = j["seqId"].get<int>();
            }
            
            if (j.contains("tools") && j["tools"].is_string()) {
                params.tools = j["tools"].get<std::string>();
            }
            
            if (j.contains("toolChoice") && j["toolChoice"].is_string()) {
                params.toolChoice = j["toolChoice"].get<std::string>();
            }
            
            return params;
        }

        /**
         * @brief Converts CompletionResult to JSON response
         */
        json completionResultToJson(const CompletionResult& result)
        {
            json response;
            
            response["tokens"] = result.tokens;
            response["text"] = result.text;
            response["tps"] = result.tps;
            response["ttft"] = result.ttft;
            
            return response;
        }

        /**
         * @brief Estimates prompt token count (simple approximation)
         */
        int estimatePromptTokens(const std::vector<Message>& messages)
        {
            int totalChars = 0;
            for (const auto& msg : messages)
            {
                totalChars += msg.content.length() + msg.role.length() + 10; // +10 for formatting
            }
            return totalChars / 4; // Rough approximation: 4 chars per token
        }
    }

    InferenceChatCompletionRoute::InferenceChatCompletionRoute() : monitor_(&CompletionMonitor::getInstance())
    {
        ServerLogger::logInfo("InferenceChatCompletionRoute initialized with completion monitoring");
    }

    InferenceChatCompletionRoute::~InferenceChatCompletionRoute() = default;

    bool InferenceChatCompletionRoute::match(const std::string& method, const std::string& path)
    {
        return (method == "POST" && (path == "/v1/inference/chat/completions" || path == "/inference/chat/completions"));
    }

    void InferenceChatCompletionRoute::handle(SocketType sock, const std::string& body)
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
            ServerLogger::logInfo("[Thread %u] Received inference chat completion request", std::this_thread::get_id());

            // Extract model name (required field)
            std::string modelName;
            if (j.contains("model") && j["model"].is_string()) {
                modelName = j["model"].get<std::string>();
            } else {
                throw std::invalid_argument("Missing or invalid 'model' field");
            }

            // Parse the chat completion parameters
            ChatCompletionParameters params = parseChatCompletionParameters(j);

            if (!params.isValid())
            {
                throw std::invalid_argument("Invalid chat completion parameters");
            }

            // Get the NodeManager and inference engine
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            auto engine = nodeManager.getEngine(modelName);

            if (!engine)
            {
                throw std::runtime_error("Model '" + modelName + "' not found or could not be loaded");
            }

            // Estimate prompt tokens for usage tracking
            int estimatedPromptTokens = estimatePromptTokens(params.messages);

            // Start completion monitoring
            requestId = monitor_->startRequest(modelName, "default");
            monitor_->recordInputTokens(requestId, estimatedPromptTokens);

            if (params.streaming)
            {
                // Handle streaming response
                ServerLogger::logInfo("[Thread %u] Processing streaming inference chat completion request for model '%s'",
                                      std::this_thread::get_id(), modelName.c_str());

                // Submit job to inference engine
                int jobId = engine->submitChatCompletionsJob(params);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit chat completion job to inference engine");
                }

                // Start the streaming response with proper SSE headers
                begin_streaming_response(sock, 200, {{"Content-Type", "text/event-stream"}, {"Cache-Control", "no-cache"}});

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
                        json errorResponse;
                        errorResponse["error"] = error;
                        errorResponse["text"] = "";
                        errorResponse["tokens"] = json::array();
                        errorResponse["tps"] = 0.0f;
                        errorResponse["ttft"] = 0.0f;

                        std::string sseData = "data: " + errorResponse.dump() + "\n\n";
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

                        // Create partial result for streaming
                        CompletionResult partialResult;
                        partialResult.text = newContent;
                        partialResult.tps = result.tps;
                        partialResult.ttft = result.ttft;
                        // Only include new tokens since last update
                        if (result.tokens.size() > static_cast<size_t>(previousText.length() / 4)) {
                            auto startIt = result.tokens.begin() + static_cast<int>(previousText.length() / 4);
                            partialResult.tokens.assign(startIt, result.tokens.end());
                        }

                        json streamResponse = completionResultToJson(partialResult);
                        streamResponse["partial"] = true;

                        // Format as SSE data message
                        std::string sseData = "data: " + streamResponse.dump() + "\n\n";
                        send_stream_chunk(sock, StreamChunk(sseData, false));

                        previousText = result.text;
                    }

                    // Brief sleep to avoid busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                // Send final result if no error occurred
                if (!engine->hasJobError(jobId))
                {
                    CompletionResult finalResult = engine->getJobResult(jobId);
                    json finalResponse = completionResultToJson(finalResult);
                    finalResponse["partial"] = false;

                    std::string sseData = "data: " + finalResponse.dump() + "\n\n";
                    send_stream_chunk(sock, StreamChunk(sseData, false));
                }

                // Send the final [DONE] marker
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
                ServerLogger::logInfo("[Thread %u] Processing non-streaming inference chat completion request for model '%s'",
                                      std::this_thread::get_id(), modelName.c_str());

                // Submit job to inference engine
                int jobId = engine->submitChatCompletionsJob(params);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit chat completion job to inference engine");
                }

                // Wait for job completion
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

                    // Record output tokens based on token count
                    int outputTokens = static_cast<int>(result.tokens.size());
                    for (int i = 0; i < outputTokens; ++i)
                    {
                        monitor_->recordOutputToken(requestId);
                    }
                }

                // Complete the monitoring
                monitor_->completeRequest(requestId);

                // Convert result to JSON and send response
                json response = completionResultToJson(result);
                send_response(sock, 200, response.dump());

                ServerLogger::logInfo("[Thread %u] Completed non-streaming response for job %d (%.2f tokens/sec)",
                                      std::this_thread::get_id(), jobId, result.tps);
            }
        }
        catch (const json::exception& ex)
        {
            // Mark request as failed if monitoring was started
            if (!requestId.empty())
            {
                monitor_->failRequest(requestId);
            }

            // Specifically handle JSON parsing errors
            ServerLogger::logError("[Thread %u] JSON parsing error: %s",
                                   std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Invalid JSON: ") + ex.what()}, {"type", "invalid_request_error"}}}};

            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception& ex)
        {
            // Mark request as failed if monitoring was started
            if (!requestId.empty())
            {
                monitor_->failRequest(requestId);
            }

            ServerLogger::logError("[Thread %u] Error handling inference chat completion: %s",
                                   std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Error: ") + ex.what()}, {"type", "invalid_request_error"}}}};

            send_response(sock, 400, jError.dump());
        }
    }

} // namespace kolosal

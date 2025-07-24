#include "kolosal/routes/oai_completions_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/models/chat_response_model.hpp"
#include "kolosal/models/chat_response_chunk_model.hpp"
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

            // Set random seed if specified
            if (request.seed.has_value())
            {
                params.randomSeed = request.seed.value();
            }

            return params;
        }

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
                        joined << "\n";
                }
                params.prompt = joined.str();
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

            // Set random seed if specified
            if (request.seed.has_value())
            {
                params.randomSeed = request.seed.value();
            }

            // Set unique sequence ID based on timestamp
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            static int seqCounter = 0;
            params.seqId = static_cast<int>(timestamp * 1000 + seqCounter++);

            return params;
        }

        /**
         * @brief Converts tokens per second and token count to usage statistics (chat)
         */
        void updateChatUsageStats(ChatCompletionResponse &response, const CompletionResult &result, int promptTokens)
        {
            response.usage.prompt_tokens = promptTokens;
            response.usage.completion_tokens = static_cast<int>(result.tokens.size());
            response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
        }

        /**
         * @brief Updates usage statistics for completion response
         */
        void updateCompletionUsageStats(CompletionResponse &response, const CompletionResult &result, int promptTokens)
        {
            response.usage.prompt_tokens = promptTokens;
            response.usage.completion_tokens = static_cast<int>(result.tokens.size());
            response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
        }

        /**
         * @brief Estimates prompt token count for chat messages (simple approximation)
         */
        int estimateChatPromptTokens(const std::vector<ChatMessage> &messages)
        {
            int totalChars = 0;
            for (const auto &msg : messages)
            {
                totalChars += static_cast<int>(msg.content.length() + msg.role.length()) + 10; // +10 for formatting
            }
            return totalChars / 4; // Rough approximation: 4 chars per token
        }

        /**
         * @brief Estimates prompt token count for text completion (simple approximation)
         */
        int estimateTextPromptTokens(const std::string &prompt)
        {
            return static_cast<int>(prompt.length()) / 4; // Rough approximation: 4 chars per token
        }
    }

    OaiCompletionsRoute::OaiCompletionsRoute()
    {
    }

    OaiCompletionsRoute::~OaiCompletionsRoute() = default;

    bool OaiCompletionsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && 
                (path == "/v1/chat/completions" || path == "/chat/completions" ||
                 path == "/v1/completions" || path == "/completions"));
    }

    void OaiCompletionsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Check for empty body
            if (body.empty())
            {
                throw std::invalid_argument("Request body is empty");
            }

            auto j = json::parse(body);
            
            // Determine the type of request based on the endpoint path
            // We need to get the path from the request context, but since it's not available in handle(),
            // we'll determine it based on the presence of 'messages' field in the JSON
            if (j.contains("messages"))
            {
                handleChatCompletion(sock, body);
            }
            else if (j.contains("prompt"))
            {
                handleTextCompletion(sock, body);
            }
            else
            {
                throw std::invalid_argument("Invalid request: missing 'messages' or 'prompt' field");
            }
        }
        catch (const json::parse_error &ex)
        {
            ServerLogger::logError("JSON parsing error: %s", ex.what());
            json jError = {{"error", {{"message", std::string("Invalid JSON: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Error handling completion request: %s", ex.what());
            json jError = {{"error", {{"message", std::string("Error: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 400, jError.dump());
        }
    }

    bool OaiCompletionsRoute::isTextCompletionPath(const std::string &path)
    {
        return (path == "/v1/completions" || path == "/completions");
    }

    bool OaiCompletionsRoute::isChatCompletionPath(const std::string &path)
    {
        return (path == "/v1/chat/completions" || path == "/chat/completions");
    }

    void OaiCompletionsRoute::handleChatCompletion(SocketType sock, const std::string &body)
    {
        try
        {
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
            int estimatedPromptTokens = estimateChatPromptTokens(request.messages);

            if (request.stream)
            {
                // Handle streaming response
                ServerLogger::logInfo("[Thread %u] Processing streaming chat completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitChatCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit job to inference engine");
                }

                // Send streaming headers
                std::string headers = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/event-stream\r\n"
                                      "Cache-Control: no-cache\r\n"
                                      "Connection: keep-alive\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Access-Control-Allow-Headers: *\r\n\r\n";

                send(sock, headers.c_str(), static_cast<int>(headers.length()), 0);

                // Poll for results and stream them
                bool jobComplete = false;
                std::string allText;

                while (!jobComplete)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    CompletionResult result = engine->getJobResult(jobId);

                    // Check if we have new text to stream
                    if (result.text.length() > allText.length())
                    {
                        // Send new characters as tokens
                        std::string newText = result.text.substr(allText.length());
                        
                        ChatCompletionChunk chunk;
                        chunk.id = "chatcmpl-" + std::to_string(jobId);
                        chunk.object = "chat.completion.chunk";
                        chunk.created = static_cast<long long>(std::time(nullptr));
                        chunk.model = request.model;

                        ChatCompletionChunkChoice choice;
                        choice.index = 0;
                        choice.delta.content = newText;
                        choice.finish_reason = "";

                        chunk.choices.push_back(choice);

                        json chunkJson = chunk.to_json();
                        std::string chunkData = "data: " + chunkJson.dump() + "\n\n";
                        send(sock, chunkData.c_str(), static_cast<int>(chunkData.length()), 0);
                        
                        allText = result.text;
                    }

                    if (engine->isJobFinished(jobId))
                    {
                        jobComplete = true;

                        // Send final chunk with finish_reason
                        ChatCompletionChunk finalChunk;
                        finalChunk.id = "chatcmpl-" + std::to_string(jobId);
                        finalChunk.object = "chat.completion.chunk";
                        finalChunk.created = static_cast<long long>(std::time(nullptr));
                        finalChunk.model = request.model;

                        ChatCompletionChunkChoice finalChoice;
                        finalChoice.index = 0;
                        finalChoice.delta.content = "";
                        finalChoice.finish_reason = "stop";

                        finalChunk.choices.push_back(finalChoice);

                        json finalChunkJson = finalChunk.to_json();
                        std::string finalChunkData = "data: " + finalChunkJson.dump() + "\n\n";
                        send(sock, finalChunkData.c_str(), static_cast<int>(finalChunkData.length()), 0);

                        // Send [DONE]
                        std::string doneData = "data: [DONE]\n\n";
                        send(sock, doneData.c_str(), static_cast<int>(doneData.length()), 0);
                    }
                }

                ServerLogger::logInfo("[Thread %u] Streaming chat completion completed for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());
            }
            else
            {
                // Handle non-streaming response
                ServerLogger::logInfo("[Thread %u] Processing non-streaming chat completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitChatCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit job to inference engine");
                }

                // Wait for completion
                CompletionResult result;
                while (!engine->isJobFinished(jobId))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    result = engine->getJobResult(jobId);
                }

                // Get final result
                result = engine->getJobResult(jobId);

                // Build response
                ChatCompletionResponse response;
                response.id = "chatcmpl-" + std::to_string(jobId);
                response.object = "chat.completion";
                response.created = static_cast<long long>(std::time(nullptr));
                response.model = request.model;

                // Build choice
                ChatCompletionChoice choice;
                choice.index = 0;
                choice.message.role = "assistant";
                choice.message.content = result.text;
                choice.finish_reason = "stop";

                response.choices.push_back(choice);

                // Update usage statistics
                updateChatUsageStats(response, result, estimatedPromptTokens);

                // Send response
                json jResponse = response.to_json();
                send_response(sock, 200, jResponse.dump());

                ServerLogger::logInfo("[Thread %u] Non-streaming chat completion completed for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Error in chat completion: %s", ex.what());
            json jError = {{"error", {{"message", std::string("Error: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 400, jError.dump());
        }
    }

    void OaiCompletionsRoute::handleTextCompletion(SocketType sock, const std::string &body)
    {
        try
        {
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

            // Build inference parameters
            CompletionParameters inferenceParams = buildCompletionParameters(request);

            // Estimate prompt tokens for usage tracking
            int estimatedPromptTokens = estimateTextPromptTokens(inferenceParams.prompt);

            if (request.stream)
            {
                // Handle streaming response
                ServerLogger::logInfo("[Thread %u] Processing streaming completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit job to inference engine");
                }

                // Send streaming headers
                std::string headers = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/event-stream\r\n"
                                      "Cache-Control: no-cache\r\n"
                                      "Connection: keep-alive\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Access-Control-Allow-Headers: *\r\n\r\n";

                send(sock, headers.c_str(), static_cast<int>(headers.length()), 0);

                // Poll for results and stream them
                bool jobComplete = false;
                std::string allText;

                while (!jobComplete)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    CompletionResult result = engine->getJobResult(jobId);

                    // Check if we have new text to stream
                    if (result.text.length() > allText.length())
                    {
                        // Send new characters
                        std::string newText = result.text.substr(allText.length());
                        
                        CompletionChunk chunk;
                        chunk.id = "cmpl-" + std::to_string(jobId);
                        chunk.object = "text_completion";
                        chunk.created = static_cast<long long>(std::time(nullptr));
                        chunk.model = request.model;

                        CompletionChunkChoice choice;
                        choice.text = newText;
                        choice.index = 0;
                        choice.finish_reason = "";

                        chunk.choices.push_back(choice);

                        json chunkJson = chunk.to_json();
                        std::string chunkData = "data: " + chunkJson.dump() + "\n\n";
                        send(sock, chunkData.c_str(), static_cast<int>(chunkData.length()), 0);
                        
                        allText = result.text;
                    }

                    if (engine->isJobFinished(jobId))
                    {
                        jobComplete = true;

                        // Send final chunk with finish_reason
                        CompletionChunk finalChunk;
                        finalChunk.id = "cmpl-" + std::to_string(jobId);
                        finalChunk.object = "text_completion";
                        finalChunk.created = static_cast<long long>(std::time(nullptr));
                        finalChunk.model = request.model;

                        CompletionChunkChoice finalChoice;
                        finalChoice.text = "";
                        finalChoice.index = 0;
                        finalChoice.finish_reason = "stop";

                        finalChunk.choices.push_back(finalChoice);

                        json finalChunkJson = finalChunk.to_json();
                        std::string finalChunkData = "data: " + finalChunkJson.dump() + "\n\n";
                        send(sock, finalChunkData.c_str(), static_cast<int>(finalChunkData.length()), 0);

                        // Send [DONE]
                        std::string doneData = "data: [DONE]\n\n";
                        send(sock, doneData.c_str(), static_cast<int>(doneData.length()), 0);
                    }
                }

                ServerLogger::logInfo("[Thread %u] Streaming completion completed for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());
            }
            else
            {
                // Handle non-streaming response
                ServerLogger::logInfo("[Thread %u] Processing non-streaming completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                int jobId = engine->submitCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    throw std::runtime_error("Failed to submit job to inference engine");
                }

                // Wait for completion
                CompletionResult result;
                while (!engine->isJobFinished(jobId))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    result = engine->getJobResult(jobId);
                }

                // Get final result
                result = engine->getJobResult(jobId);

                // Build response
                CompletionResponse response;
                response.id = "cmpl-" + std::to_string(jobId);
                response.object = "text_completion";
                response.created = static_cast<long long>(std::time(nullptr));
                response.model = request.model;

                // Build choice
                CompletionChoice choice;
                choice.index = 0;
                choice.text = result.text;
                choice.finish_reason = "stop";

                response.choices.push_back(choice);

                // Update usage statistics
                updateCompletionUsageStats(response, result, estimatedPromptTokens);

                // Send response
                json jResponse = response.to_json();
                send_response(sock, 200, jResponse.dump());

                ServerLogger::logInfo("[Thread %u] Non-streaming completion completed for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Error in text completion: %s", ex.what());
            json jError = {{"error", {{"message", std::string("Error: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 400, jError.dump());
        }
    }

} // namespace kolosal

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
#include "kolosal/image_utils.hpp"

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
#include <future>

using json = nlohmann::json;

namespace kolosal
{
    namespace
    {
        /**
         * @brief Process images from chat messages asynchronously
         * @param msg The chat message containing potential images
         * @return Future containing vector of processed ImageData
         */
        std::future<std::vector<ImageData>> processImagesAsync(const ChatMessage& msg) {
            return std::async(std::launch::async, [msg]() {
                std::vector<ImageData> processedImages;
                
                try {
                    if (!msg.hasImages()) {
                        return processedImages;
                    }
                    
                    auto imageUrls = msg.getImageUrls();
                    ServerLogger::logDebug("Processing %zu images for message", imageUrls.size());
                    
                    for (const auto& [url, detail] : imageUrls) {
                        try {
                            std::vector<unsigned char> imageData;
                            std::string format;
                            
                            if (ImageUtils::isDataUrl(url)) {
                                // Handle base64 data URLs
                                auto [data, fmt] = ImageUtils::decodeBase64Image(url);
                                imageData = data;
                                format = fmt;
                            } else if (ImageUtils::isHttpUrl(url)) {
                                // Handle HTTP(S) URLs - use the async download method
                                auto future = ImageUtils::downloadImageAsync(url);
                                auto [data, fmt] = future.get();
                                imageData = data;
                                format = fmt;
                            } else {
                                // Handle local file paths
                                auto [data, fmt] = ImageUtils::loadImageFromFile(url);
                                imageData = data;
                                format = fmt;
                            }
                            
                            if (!ImageUtils::isSupportedFormat(format)) {
                                ServerLogger::logWarning("Unsupported image format: %s", format.c_str());
                                continue;
                            }
                            
                            ImageData imgData(imageData, format, url);
                            imgData.detail = detail;
                            processedImages.push_back(imgData);
                            
                            ServerLogger::logDebug("Processed image: %s (format: %s, size: %zu bytes)", 
                                                  url.c_str(), format.c_str(), imageData.size());
                            
                        } catch (const std::exception& e) {
                            ServerLogger::logError("Failed to process image %s: %s", url.c_str(), e.what());
                            // Continue processing other images
                        }
                    }
                    
                } catch (const std::exception& e) {
                    ServerLogger::logError("Error in processImagesAsync: %s", e.what());
                }
                
                return processedImages;
            });
        }
        
        /**
         * @brief Builds ChatCompletionParameters from a ChatCompletionRequest
         * Following the ModelManager pattern from the example
         */
        ChatCompletionParameters buildChatCompletionParameters(const ChatCompletionRequest &request)
        {
            ServerLogger::logDebug("=== buildChatCompletionParameters START ===");
            ChatCompletionParameters params;

            try {
                ServerLogger::logDebug("Processing %zu messages", request.messages.size());
                
                // Process messages with vision support
                for (size_t i = 0; i < request.messages.size(); ++i)
                {
                    ServerLogger::logDebug("Processing message %zu", i);
                    const auto& msg = request.messages[i];
                    std::string textContent = msg.getTextContent();
                    ServerLogger::logDebug("Message %zu text content length: %zu", i, textContent.length());
                    
                    // Check if message has images
                    if (msg.hasImages())
                    {
                        ServerLogger::logDebug("Message %zu has %zu images", i, msg.getImageUrls().size());
                        
                        // Process images
                        std::vector<ImageData> images;
                        for (const auto& imageUrlPair : msg.getImageUrls())
                        {
                            const std::string& imageUrl = imageUrlPair.first;
                            const std::string& detail = imageUrlPair.second;
                            
                            ServerLogger::logDebug("Processing image URL: %s", imageUrl.c_str());
                            
                            // Create ImageData from URL
                            ImageData imageData;
                            imageData.url = imageUrl;
                            imageData.detail = detail;
                            
                            // For now, we'll create placeholder image data
                            // In a real implementation, this would download and decode the image
                            if (imageUrl.find("data:image") == 0)
                            {
                                // Base64 encoded image
                                imageData.format = "base64";
                                // Extract base64 data (simplified)
                                size_t comma_pos = imageUrl.find(',');
                                if (comma_pos != std::string::npos)
                                {
                                    std::string base64_data = imageUrl.substr(comma_pos + 1);
                                    // For now, store as-is - proper implementation would decode
                                    imageData.data.assign(base64_data.begin(), base64_data.end());
                                }
                            }
                            else
                            {
                                // URL to image
                                imageData.format = "url";
                                // For now, create placeholder data
                                imageData.data = std::vector<uint8_t>(224*224*3, 128); // Gray placeholder
                            }
                            
                            images.push_back(imageData);
                        }
                        
                        // Create message with images
                        ServerLogger::logDebug("Creating message with %zu images", images.size());
                        Message imageMessage(msg.role, textContent, images);
                        ServerLogger::logDebug("Message with images created successfully");
                        
                        params.messages.push_back(imageMessage);
                        ServerLogger::logDebug("Message with images added to params.messages successfully");
                    }
                    else
                    {
                        // Text-only message
                        ServerLogger::logDebug("Creating text-only message for role: %s", msg.role.c_str());
                        
                        std::vector<ImageData> emptyImages;
                        ServerLogger::logDebug("Empty images vector created");
                        
                        // Create Message object explicitly instead of using emplace_back
                        ServerLogger::logDebug("Creating Message object...");
                        Message textMessage(msg.role, textContent, emptyImages);
                        ServerLogger::logDebug("Message object created successfully");
                        
                        ServerLogger::logDebug("Adding message to params.messages...");
                        params.messages.push_back(textMessage);
                        ServerLogger::logDebug("Message added to params.messages successfully");
                    }
                    
                    ServerLogger::logDebug("Message %zu processed successfully", i);
                }
                ServerLogger::logDebug("Message processing phase completed");

                // Set generation parameters
                ServerLogger::logDebug("Setting generation parameters");
                params.temperature = static_cast<float>(request.temperature);
                params.topP = static_cast<float>(request.top_p);
                params.streaming = request.stream;
                params.maxNewTokens = request.max_tokens.value_or(100);
                params.randomSeed = request.seed.value_or(42);
                
                ServerLogger::logDebug("Parameters set: temp=%f, topP=%f, streaming=%d, maxTokens=%d, seed=%d", 
                                     params.temperature, params.topP, params.streaming, params.maxNewTokens, params.randomSeed);
                
                ServerLogger::logDebug("=== buildChatCompletionParameters SUCCESS ===");
                return params;
                
            } catch (const std::exception& e) {
                ServerLogger::logError("Error building chat completion parameters: %s", e.what());
                throw;
            }
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
                totalChars += static_cast<int>(msg.getTextContent().length() + msg.role.length()) + 10; // +10 for formatting
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
            ServerLogger::logInfo("=== OaiCompletionsRoute::handle called ===");
            ServerLogger::logInfo("Request body length: %zu", body.length());
            ServerLogger::logInfo("Request body content: %s", body.c_str());
            
            // Check for empty body
            if (body.empty())
            {
                ServerLogger::logError("Request body is empty");
                throw std::invalid_argument("Request body is empty");
            }

            auto j = json::parse(body);
            ServerLogger::logInfo("JSON parsed successfully");
            
            // Determine the type of request based on the endpoint path
            // We need to get the path from the request context, but since it's not available in handle(),
            // we'll determine it based on the presence of 'messages' field in the JSON
            if (j.contains("messages"))
            {
                ServerLogger::logInfo("Detected chat completion request");
                handleChatCompletion(sock, body);
            }
            else if (j.contains("prompt"))
            {
                ServerLogger::logInfo("Detected text completion request");
                handleTextCompletion(sock, body);
            }
            else
            {
                ServerLogger::logError("Invalid request: missing 'messages' or 'prompt' field");
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
            ServerLogger::logInfo("=== CHAT COMPLETION DEBUG START ===");
            
            auto j = json::parse(body);
            ServerLogger::logInfo("[Thread %u] Received chat completion request", std::this_thread::get_id());
            ServerLogger::logDebug("Request body parsed successfully");

            // Parse the request
            ServerLogger::logDebug("=== PARSING REQUEST START ===");
            ServerLogger::logDebug("Creating ChatCompletionRequest object");
            ChatCompletionRequest request;
            
            ServerLogger::logDebug("=== CALLING request.from_json() ===");
            request.from_json(j);
            ServerLogger::logDebug("=== request.from_json() COMPLETED ===");
            ServerLogger::logDebug("Request parsed successfully");

            ServerLogger::logDebug("=== VALIDATING REQUEST ===");
            if (!request.validate())
            {
                ServerLogger::logError("Request validation failed");
                throw std::invalid_argument("Invalid request parameters");
            }
            ServerLogger::logDebug("=== REQUEST VALIDATION PASSED ===");
            ServerLogger::logDebug("Request validated successfully");

            ServerLogger::logDebug("Chat completion request: model=%s, stream=%d, messages=%zu", 
                                  request.model.c_str(), request.stream, request.messages.size());

            // Get the NodeManager and inference engine
            ServerLogger::logDebug("=== GETTING NODE MANAGER ===");
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            ServerLogger::logDebug("=== NODE MANAGER OBTAINED ===");
            
            ServerLogger::logDebug("=== GETTING ENGINE FOR MODEL ===");
            ServerLogger::logDebug("Getting engine for model: %s", request.model.c_str());
            auto engine = nodeManager.getEngine(request.model);
            ServerLogger::logDebug("=== ENGINE RETRIEVAL COMPLETED ===");

            if (!engine)
            {
                ServerLogger::logError("Engine not found for model: %s", request.model.c_str());
                throw std::runtime_error("Model '" + request.model + "' not found or could not be loaded");
            }
            ServerLogger::logDebug("=== ENGINE OBTAINED SUCCESSFULLY ===");

            // Build inference parameters following ModelManager pattern
            ServerLogger::logDebug("Building inference parameters...");
            ServerLogger::logDebug("=== ABOUT TO CALL buildChatCompletionParameters ===");
            
            ChatCompletionParameters inferenceParams;
            
            try {
                ServerLogger::logDebug("Calling buildChatCompletionParameters with request...");
                inferenceParams = buildChatCompletionParameters(request);
                ServerLogger::logDebug("=== buildChatCompletionParameters COMPLETED SUCCESSFULLY ===");
                ServerLogger::logDebug("Inference parameters built successfully");
            } catch (const std::exception& e) {
                ServerLogger::logError("=== buildChatCompletionParameters FAILED ===");
                ServerLogger::logError("Failed to build inference parameters: %s", e.what());
                throw;
            }
            
            ServerLogger::logDebug("=== INFERENCE PARAMETERS READY ===");
            ServerLogger::logDebug("Parameters ready - messages count: %zu", inferenceParams.messages.size());

            // Estimate prompt tokens for usage tracking
            ServerLogger::logDebug("Estimating prompt tokens");
            int estimatedPromptTokens = estimateChatPromptTokens(request.messages);
            ServerLogger::logDebug("Estimated prompt tokens: %d", estimatedPromptTokens);

            ServerLogger::logInfo("=== ABOUT TO SUBMIT TO INFERENCE ENGINE ===");

            if (request.stream)
            {
                // Handle streaming response
                ServerLogger::logInfo("[Thread %u] Processing streaming chat completion request for model '%s'",
                                      std::this_thread::get_id(), request.model.c_str());

                // Submit job to inference engine
                ServerLogger::logDebug("Submitting job to inference engine...");
                int jobId = engine->submitChatCompletionsJob(inferenceParams);

                if (jobId < 0)
                {
                    ServerLogger::logError("Failed to submit job to inference engine, jobId: %d", jobId);
                    throw std::runtime_error("Failed to submit job to inference engine");
                }
                
                ServerLogger::logInfo("Job submitted successfully with ID: %d", jobId);

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
                ServerLogger::logDebug("Submitting non-streaming job to inference engine...");
                ServerLogger::logDebug("About to call engine->submitChatCompletionsJob with parameters:");
                ServerLogger::logDebug("  - messages count: %zu", inferenceParams.messages.size());
                ServerLogger::logDebug("  - temperature: %f", inferenceParams.temperature);
                ServerLogger::logDebug("  - topP: %f", inferenceParams.topP);
                ServerLogger::logDebug("  - streaming: %d", inferenceParams.streaming);
                ServerLogger::logDebug("  - maxNewTokens: %d", inferenceParams.maxNewTokens);
                ServerLogger::logDebug("  - randomSeed: %d", inferenceParams.randomSeed);
                
                ServerLogger::logInfo("=== CALLING submitChatCompletionsJob NOW ===");
                
                // Additional debug logging to isolate the crash
                ServerLogger::logDebug("About to log engine pointer address");
                ServerLogger::logDebug("Engine pointer: %p", engine);
                
                ServerLogger::logDebug("About to validate engine is not null");
                if (!engine) {
                    ServerLogger::logError("Engine pointer is null!");
                    throw std::runtime_error("Engine pointer is null");
                }
                
                ServerLogger::logDebug("About to log inference params details");
                ServerLogger::logDebug("inferenceParams.messages.size(): %zu", inferenceParams.messages.size());
                
                ServerLogger::logDebug("About to check if messages vector is valid");
                if (inferenceParams.messages.empty()) {
                    ServerLogger::logError("Messages vector is empty!");
                    throw std::runtime_error("Messages vector is empty");
                }
                
                ServerLogger::logDebug("About to call engine->submitChatCompletionsJob...");
                int jobId = engine->submitChatCompletionsJob(inferenceParams);
                ServerLogger::logInfo("=== submitChatCompletionsJob RETURNED ===");

                if (jobId < 0)
                {
                    ServerLogger::logError("Failed to submit non-streaming job to inference engine, jobId: %d", jobId);
                    throw std::runtime_error("Failed to submit job to inference engine");
                }
                
                ServerLogger::logInfo("Non-streaming job submitted successfully with ID: %d", jobId);

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

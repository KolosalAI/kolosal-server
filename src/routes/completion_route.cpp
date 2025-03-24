#pragma once

#include "kolosal/routes/completion_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/models/completion_response_model.hpp"
#include "kolosal/models/completion_response_chunk_model.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>

using json = nlohmann::json;

namespace kolosal {

    bool CompletionsRoute::match(const std::string& method, const std::string& path) {
        return (method == "POST" && (path == "/v1/completions" || path == "/completions"));
    }

    void CompletionsRoute::handle(SocketType sock, const std::string& body) {
        try {
            // Check for empty body
            if (body.empty()) {
                throw std::invalid_argument("Request body is empty");
            }

            auto j = json::parse(body);
            Logger::logInfo("[Thread %u] Received completion request", std::this_thread::get_id());

            // Parse the request
            CompletionRequest request;
            request.from_json(j);

            if (!request.validate()) {
                throw std::invalid_argument("Invalid request parameters");
            }

            if (request.stream) {
                // Handle streaming response
                Logger::logInfo("[Thread %u] Processing streaming completion request", std::this_thread::get_id());

                // Create a persistent ID for this completion
                std::string completionId = "cmpl-" + std::to_string(std::time(nullptr));

                // Start the streaming response with proper SSE headers
                begin_streaming_response(sock, 200, {
                    {"Content-Type", "text/event-stream"},
                    {"Cache-Control", "no-cache"}
                    });

                // Get the streaming callback
                auto streamingCallback = ServerAPI::instance().getCompletionStreamingCallback();

                // Process streaming generation
                int chunkIndex = 0;
                bool hasMoreChunks = true;

                while (hasMoreChunks) {
                    CompletionChunk chunk;

                    // Call the callback to get the next chunk
                    hasMoreChunks = streamingCallback(request, completionId, chunkIndex, chunk);

                    // Format as SSE data message (this is crucial for OpenAI client)
                    std::string sseData = "data: " + chunk.to_json().dump() + "\n\n";

                    // Send the chunk to the client
                    send_stream_chunk(sock, StreamChunk(sseData, false));

                    chunkIndex++;
                }

                // Send the final [DONE] marker required by OpenAI client
                send_stream_chunk(sock, StreamChunk("data: [DONE]\n\n", false));

                // Then terminate the stream
                send_stream_chunk(sock, StreamChunk("", true));

                Logger::logInfo("[Thread %u] Completed streaming response with %d chunks",
                    std::this_thread::get_id(), chunkIndex);
            }
            else {
                // Handle normal (non-streaming) response
                Logger::logInfo("[Thread %u] Processing non-streaming completion request",
                    std::this_thread::get_id());

                // Get the inference callback
                auto inferenceCallback = ServerAPI::instance().getCompletionCallback();

                // Call the callback to generate the response
                CompletionResponse response = inferenceCallback(request);

                if (!response.error.empty()) {
                    Logger::logError("[Thread %u] Inference error: %s",
                        std::this_thread::get_id(), response.error.c_str());

                    json jError = { {"error", {
                        {"message", response.error},
                        {"type", "invalid_request_error"},
                        {"param", nullptr},
                        {"code", nullptr}
                    }} };

                    send_response(sock, 400, jError.dump());
                    return;
                }

                // Send the response
                send_response(sock, 200, response.to_json().dump());

                Logger::logInfo("[Thread %u] Completed non-streaming response",
                    std::this_thread::get_id());
            }
        }
        catch (const json::exception& ex) {
            // Specifically handle JSON parsing errors
            Logger::logError("[Thread %u] JSON parsing error: %s",
                std::this_thread::get_id(), ex.what());

            json jError = { {"error", {
                {"message", std::string("Invalid JSON: ") + ex.what()},
                {"type", "invalid_request_error"},
                {"param", nullptr},
                {"code", nullptr}
            }} };

            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception& ex) {
            Logger::logError("[Thread %u] Error handling completion: %s",
                std::this_thread::get_id(), ex.what());

            json jError = { {"error", {
                {"message", std::string("Error: ") + ex.what()},
                {"type", "invalid_request_error"},
                {"param", nullptr},
                {"code", nullptr}
            }} };

            send_response(sock, 400, jError.dump());
        }
    }

} // namespace kolosal
#pragma once

#include "kolosal/routes/chat_completion_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/models/chat_response_model.hpp"
#include "kolosal/models/chat_response_chunk_model.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace kolosal {

    bool ChatCompletionsRoute::match(const std::string& method, const std::string& path) {
        return (method == "POST" && path == "/v1/chat/completions");
    }

    void ChatCompletionsRoute::handle(SocketType sock, const std::string& body) {
        try {
            auto j = json::parse(body);
            Logger::logInfo("Received chat completion request");

            // Parse the request
            ChatCompletionRequest request;
            request.from_json(j);

            if (!request.validate()) {
                throw std::invalid_argument("Invalid request body");
            }

            if (request.stream) {
                // Handle streaming response
                Logger::logInfo("Processing streaming chat completion request");

                // Create a persistent ID for this completion
                std::string completionId = "chatcmpl-" + std::to_string(std::time(nullptr));

                // Start the streaming response
                begin_streaming_response(sock, 200);

                // Get the streaming callback
                auto streamingCallback = ServerAPI::instance().getStreamingInferenceCallback();

                // Process streaming generation
                int chunkIndex = 0;
                bool hasMoreChunks = true;

                while (hasMoreChunks) {
                    ChatCompletionChunk chunk;

                    // Call the callback to get the next chunk
                    hasMoreChunks = streamingCallback(request, completionId, chunkIndex, chunk);

                    // Send the chunk to the client
                    send_stream_chunk(sock, StreamChunk(chunk.to_json().dump(), false));

                    chunkIndex++;
                }

                // Send the final empty chunk to terminate the stream
                send_stream_chunk(sock, StreamChunk("", true));

                Logger::logInfo("Completed streaming response with %d chunks", chunkIndex);
            }
            else {
                // Handle normal (non-streaming) response
                Logger::logInfo("Processing non-streaming chat completion request");

                // Get the inference callback
                auto inferenceCallback = ServerAPI::instance().getInferenceCallback();

                // Call the callback to generate the response
                ChatCompletionResponse response = inferenceCallback(request);

                // Send the response
                send_response(sock, 200, response.to_json().dump());

                Logger::logInfo("Completed non-streaming response");
            }
        }
        catch (const std::exception& ex) {
            Logger::logError("Error handling chat completion: %s", ex.what());

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
#pragma once

#include "kolosal/routes/completion_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/models/completion_request_model.hpp"
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
            ServerLogger::logInfo("[Thread %u] Received completion request", std::this_thread::get_id());

            // Parse the request
            CompletionRequest request;
            request.from_json(j);

            if (!request.validate()) {
                throw std::invalid_argument("Invalid request parameters");
            }

            if (request.stream) {
                // Handle streaming response
                ServerLogger::logInfo("[Thread %u] Processing streaming completion request", std::this_thread::get_id());

                // Create a persistent ID for this completion
                std::string completionId = "cmpl-" + std::to_string(std::time(nullptr));                // Start the streaming response with proper SSE headers
                begin_streaming_response(sock, 200, {
                    {"Content-Type", "text/event-stream"},
                    {"Cache-Control", "no-cache"}
                    });

                // Process streaming generation with fixed response
                int chunkIndex = 0;
                bool hasMoreChunks = true;

                while (hasMoreChunks) {
                    CompletionChunk chunk;
                    chunk.id = completionId;
                    chunk.model = request.model;

                    CompletionChunkChoice choice;
                    choice.index = 0;

                    if (chunkIndex == 0) {
                        // First chunk
                        choice.text = "This is a ";
                        choice.finish_reason = "";
                    }
                    else if (chunkIndex == 1) {
                        // Second chunk
                        choice.text = "default completion ";
                        choice.finish_reason = "";
                    }
                    else if (chunkIndex == 2) {
                        // Third chunk
                        choice.text = "response from kolosal-server.";
                        choice.finish_reason = "stop";
                    }

                    chunk.choices.push_back(choice);

                    // Format as SSE data message (this is crucial for OpenAI client)
                    std::string sseData = "data: " + chunk.to_json().dump() + "\n\n";

                    // Send the chunk to the client
                    send_stream_chunk(sock, StreamChunk(sseData, false));

                    chunkIndex++;
                    hasMoreChunks = chunkIndex < 3;
                }

                // Send the final [DONE] marker required by OpenAI client
                send_stream_chunk(sock, StreamChunk("data: [DONE]\n\n", false));

                // Then terminate the stream
                send_stream_chunk(sock, StreamChunk("", true));

                ServerLogger::logInfo("[Thread %u] Completed streaming response with %d chunks",
                    std::this_thread::get_id(), chunkIndex);
            }
            else {                // Handle normal (non-streaming) response
                ServerLogger::logInfo("[Thread %u] Processing non-streaming completion request",
                    std::this_thread::get_id());

                // Generate fixed response
                CompletionResponse response;
                response.model = request.model;

                CompletionChoice choice;
                choice.index = 0;
                choice.text = "This is a default completion response from kolosal-server.";
                choice.finish_reason = "stop";

                response.choices.push_back(choice);                // Simple token count estimation
                response.usage.prompt_tokens = 10;
                response.usage.completion_tokens = 10;
                response.usage.total_tokens = 20;

                // Send the response
                send_response(sock, 200, response.to_json().dump());

                ServerLogger::logInfo("[Thread %u] Completed non-streaming response",
                    std::this_thread::get_id());
            }
        }
        catch (const json::exception& ex) {
            // Specifically handle JSON parsing errors
            ServerLogger::logError("[Thread %u] JSON parsing error: %s",
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
            ServerLogger::logError("[Thread %u] Error handling completion: %s",
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
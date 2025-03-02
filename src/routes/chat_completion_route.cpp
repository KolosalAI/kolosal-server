#include "routes/chat_completion_route.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "models/chat_response_model.hpp"
#include "models/chat_response_chunk_model.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

using json = nlohmann::json;

bool ChatCompletionsRoute::match(const std::string& method, const std::string& path) {
    return (method == "POST" && (path == "/v1/chat/completions" || path == "/chat/completions"));
}

void ChatCompletionsRoute::handle(SocketType sock, const std::string& body) {
    try {
        auto j = json::parse(body);

        // Parse the request
        ChatCompletionRequest request;
        request.from_json(j);

        if (!request.validate()) {
            throw std::invalid_argument("Invalid request body");
        }

        // Generate a response based on the request
        std::string responseText = generateResponse(request);

        if (request.stream) {
            // Handle streaming response
            Logger::logDebug("Sending streaming chat completion response");

            // Create a persistent ID for this completion
            std::string completionId = "chatcmpl-" + std::to_string(std::time(nullptr));

            // Start the streaming response
            begin_streaming_response(sock, 200);

            // First, send the role
            ChatCompletionChunk firstChunk;
            firstChunk.id = completionId;
            firstChunk.model = request.model;

            ChatCompletionChunkChoice choice;
            choice.index = 0;
            choice.delta.role = "assistant";
            choice.finish_reason = "";

            firstChunk.choices.push_back(choice);

            send_stream_chunk(sock, StreamChunk(firstChunk.to_json().dump(), false));

            // Now tokenize the response (simulated here - in a real LLM you'd get tokens from your model)
            std::vector<std::string> tokens;
            std::istringstream iss(responseText);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }

            // Send content tokens in chunks
            for (size_t i = 0; i < tokens.size(); i++) {
                ChatCompletionChunk chunk;
                chunk.id = completionId;
                chunk.model = request.model;

                ChatCompletionChunkChoice contentChoice;
                contentChoice.index = 0;
                contentChoice.delta.content = tokens[i];
                if (i < tokens.size() - 1) {
                    contentChoice.delta.content += " ";
                }
                contentChoice.finish_reason = "";

                chunk.choices.push_back(contentChoice);

                send_stream_chunk(sock, StreamChunk(chunk.to_json().dump(), false));

                // Simulate thinking time
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Send the final completion message
            ChatCompletionChunk finalChunk;
            finalChunk.id = completionId;
            finalChunk.model = request.model;

            ChatCompletionChunkChoice finalChoice;
            finalChoice.index = 0;
            finalChoice.delta = {}; // Empty delta
            finalChoice.finish_reason = "stop";

            finalChunk.choices.push_back(finalChoice);

            send_stream_chunk(sock, StreamChunk(finalChunk.to_json().dump(), true));
        }
        else {
            // Handle normal (non-streaming) response
            Logger::logDebug("Sending non-streaming chat completion response");

            ChatCompletionResponse response;
            response.model = request.model;

            // Add the generated message
            ChatCompletionChoice choice;
            choice.index = 0;
            choice.message.role = "assistant";
            choice.message.content = responseText;
            choice.finish_reason = "stop";

            response.choices.push_back(choice);

            // Set usage statistics
            int promptTokens = 0;
            for (const auto& msg : request.messages) {
                promptTokens += countTokens(msg.content);
            }

            int completionTokens = countTokens(responseText);

            response.usage.prompt_tokens = promptTokens;
            response.usage.completion_tokens = completionTokens;
            response.usage.total_tokens = promptTokens + completionTokens;

            // Send the response
            send_response(sock, 200, response.to_json().dump());
        }
    }
    catch (const std::exception& ex) {
        json jError = { {"error", {
          {"message", std::string("Error: ") + ex.what()},
          {"type", "invalid_request_error"},
          {"param", nullptr},
          {"code", nullptr}
        }} };
        send_response(sock, 400, jError.dump());
    }
}

std::string ChatCompletionsRoute::generateResponse(const ChatCompletionRequest& request) {
    // Simple response generator based on the last user message
    std::string lastUserContent;

    // Find the last user message
    for (auto it = request.messages.rbegin(); it != request.messages.rend(); ++it) {
        if (it->role == "user") {
            lastUserContent = it->content;
            break;
        }
    }

    // Generate a simple response
    if (lastUserContent.empty()) {
        return "I'm here to help. What can I assist you with?";
    }

    if (lastUserContent.find("hello") != std::string::npos ||
        lastUserContent.find("hi") != std::string::npos) {
        return "Hello there! How can I assist you today?";
    }

    if (lastUserContent.find("?") != std::string::npos) {
        return "That's an interesting question. I'd say it depends on various factors, but I'd be happy to explore this topic with you in more detail.";
    }

    return "I understand what you're saying. Would you like me to elaborate on any particular aspect of this topic?";
}

int ChatCompletionsRoute::countTokens(const std::string& text) {
    // This is a very simplistic token counter that just counts words
    // In a real implementation, you'd use a proper tokenizer
    int count = 0;
    std::istringstream iss(text);
    std::string token;

    while (iss >> token) {
        count++;
    }

    return count;
}

#include "kolosal_server.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace kolosal;

// Custom non-streaming inference implementation
ChatCompletionResponse customInference(const ChatCompletionRequest& request) {
    ChatCompletionResponse response;
    response.model = request.model;

    // Find the last user message
    std::string lastUserContent;
    for (auto it = request.messages.rbegin(); it != request.messages.rend(); ++it) {
        if (it->role == "user") {
            lastUserContent = it->content;
            break;
        }
    }

    // Generate a custom response
    ChatCompletionChoice choice;
    choice.index = 0;
    choice.message.role = "assistant";
    choice.message.content = "Custom response to: " + lastUserContent;
    choice.finish_reason = "stop";

    response.choices.push_back(choice);

    // Set usage statistics
    response.usage.prompt_tokens = 10;
    response.usage.completion_tokens = 8;
    response.usage.total_tokens = 18;

    return response;
}

// Custom streaming inference implementation
bool customStreamingInference(const ChatCompletionRequest& request, const std::string& requestId, int chunkIndex, ChatCompletionChunk& outputChunk) {
    // delay to show the streaming response
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create a fixed set of chunks
    const std::vector<std::string> tokens = {
        "This", "is", "a", "custom", "streaming", "response", "from", "our", "application."
    };

    outputChunk.id = requestId;
    outputChunk.model = request.model;

    ChatCompletionChunkChoice choice;
    choice.index = 0;

    if (chunkIndex == 0) {
        // First chunk should include role
        choice.delta.role = "assistant";
        choice.finish_reason = "";
    }
    else if (chunkIndex <= tokens.size()) {
        // Content chunks
        choice.delta.content = tokens[chunkIndex - 1];
        if (chunkIndex < tokens.size()) {
            choice.delta.content += " ";
        }
        choice.finish_reason = "";
    }
    else {
        // Last chunk
        choice.delta.content = "";
        choice.finish_reason = "stop";
    }

    outputChunk.choices.push_back(choice);

    // Return true if there are more chunks to send
    return chunkIndex < tokens.size() + 1;
}

int main() {
    // Initialize logger
    Logger::instance().setLogFile("server.log");
    Logger::instance().setLevel(LogLevel::DEBUG);
    Logger::logInfo("Starting example application");

    // Set custom inference callbacks
    ServerAPI::instance().setInferenceCallback(customInference);
    ServerAPI::instance().setStreamingInferenceCallback(customStreamingInference);

    // Start the server
    if (!ServerAPI::instance().init("8080")) {
        Logger::logError("Failed to start server");
        return 1;
    }

    Logger::logInfo("Server started on port 8080");
    Logger::logInfo("Press Ctrl+C to exit");

    // Keep the main thread alive
    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& ex) {
        Logger::logError("Error: %s", ex.what());
    }

    // Cleanup
    ServerAPI::instance().shutdown();
    Logger::logInfo("Server shut down, exiting application");

    return 0;
}

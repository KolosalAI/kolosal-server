#pragma once

#include <string>
#include <functional>
#include <memory>

#include "export.hpp"
#include "models/chat_request_model.hpp"
#include "models/chat_response_model.hpp"
#include "models/chat_response_chunk_model.hpp"
#include "models/completion_request_model.hpp"
#include "models/completion_response_model.hpp"
#include "models/completion_response_chunk_model.hpp"

namespace kolosal {

    // Callback type for non-streaming chat completion
    using ChatCompletionCallback = std::function<ChatCompletionResponse(const ChatCompletionRequest&)>;

    // Callback type for streaming chat completion - returns one chunk at a time
    using ChatCompletionStreamingCallback = std::function<bool(const ChatCompletionRequest&,
        const std::string& requestId,
        int chunkIndex,
        ChatCompletionChunk& outputChunk)>;

    // Callback type for non-streaming completion - FIXED: should return CompletionResponse
    using CompletionCallback = std::function<CompletionResponse(const CompletionRequest&)>;

    // Callback type for streaming completion - returns one chunk at a time
    using CompletionStreamingCallback = std::function<bool(const CompletionRequest&,
        const std::string& requestId,
        int chunkIndex,
        CompletionChunk& outputChunk)>;

    // Callback type to get all models
	using GetModelsCallback = std::function<std::vector<std::string>()>;

    class KOLOSAL_SERVER_API ServerAPI {
    public:
        // Singleton pattern
        static ServerAPI& instance();

        // Delete copy/move constructors and assignments
        ServerAPI(const ServerAPI&) = delete;
        ServerAPI& operator=(const ServerAPI&) = delete;
        ServerAPI(ServerAPI&&) = delete;
        ServerAPI& operator=(ServerAPI&&) = delete;

        // Initialize and start server
        bool init(const std::string& port);
        void shutdown();

        // Set callbacks for inference
        void setChatCompletionCallback(ChatCompletionCallback callback);
        void setChatCompletionStreamingCallback(ChatCompletionStreamingCallback callback);
        void setCompletionCallback(CompletionCallback callback);
        void setCompletionStreamingCallback(CompletionStreamingCallback callback);
        void setGetModelsCallback(GetModelsCallback callback);

        // Access to currently registered callbacks
        ChatCompletionCallback getChatCompletionCallback() const;
        ChatCompletionStreamingCallback getChatCompletionStreamingCallback() const;
        CompletionCallback getCompletionCallback() const;
        CompletionStreamingCallback getCompletionStreamingCallback() const;
		GetModelsCallback getGetModelsCallback() const;

    private:
        ServerAPI();
        ~ServerAPI();

        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace kolosal
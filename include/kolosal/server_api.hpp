#pragma once

#include <string>
#include <functional>
#include <memory>

#include "export.hpp"
#include "models/chat_request_model.hpp"
#include "models/chat_response_model.hpp"
#include "models/chat_response_chunk_model.hpp"

namespace kolosal {

    // Callback type for non-streaming inference
    using InferenceCallback = std::function<ChatCompletionResponse(const ChatCompletionRequest&)>;

    // Callback type for streaming inference - returns one chunk at a time
    using StreamingInferenceCallback = std::function<bool(const ChatCompletionRequest&,
        const std::string& requestId,
        int chunkIndex,
        ChatCompletionChunk& outputChunk)>;

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
        void setInferenceCallback(InferenceCallback callback);
        void setStreamingInferenceCallback(StreamingInferenceCallback callback);

        // Access to currently registered callbacks
        InferenceCallback getInferenceCallback() const;
        StreamingInferenceCallback getStreamingInferenceCallback() const;

    private:
        ServerAPI();
        ~ServerAPI();

        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace kolosal
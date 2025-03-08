#include "kolosal/server_api.hpp"
#include "kolosal/server.hpp"
#include "kolosal/routes/chat_completion_route.hpp"
#include "kolosal/logger.hpp"
#include <memory>
#include <stdexcept>

namespace kolosal {

    // Default inference implementation (returns a simple not-implemented response)
    ChatCompletionResponse defaultInference(const ChatCompletionRequest& request) {
        ChatCompletionResponse response;
        response.model = request.model;

        ChatCompletionChoice choice;
        choice.index = 0;
        choice.message.role = "assistant";
        choice.message.content = "No inference implementation has been registered.";
        choice.finish_reason = "stop";

        response.choices.push_back(choice);

        // Simple token count estimation
        response.usage.prompt_tokens = 10;
        response.usage.completion_tokens = 10;
        response.usage.total_tokens = 20;

        return response;
    }

    // Default streaming inference (returns a simple not-implemented message in chunks)
    bool defaultStreamingInference(const ChatCompletionRequest& request,
        const std::string& requestId,
        int chunkIndex,
        ChatCompletionChunk& outputChunk) {
        outputChunk.id = requestId;
        outputChunk.model = request.model;

        ChatCompletionChunkChoice choice;
        choice.index = 0;

        if (chunkIndex == 0) {
            // First chunk should include role
            choice.delta.role = "assistant";
            choice.finish_reason = "";
        }
        else if (chunkIndex == 1) {
            // Second chunk with content
            choice.delta.content = "No streaming inference implementation registered.";
            choice.finish_reason = "";
        }
        else {
            // Last chunk
            choice.delta.content = "";
            choice.finish_reason = "stop";
        }

        outputChunk.choices.push_back(choice);

        // Return false when finished
        return chunkIndex < 2;
    }

    class ServerAPI::Impl {
    public:
        std::unique_ptr<Server> server;
        InferenceCallback inferenceCallback;
        StreamingInferenceCallback streamingInferenceCallback;

        Impl()
            : inferenceCallback(defaultInference),
            streamingInferenceCallback(defaultStreamingInference) {
        }
    };

    ServerAPI::ServerAPI() : pImpl(std::make_unique<Impl>()) {}

    ServerAPI::~ServerAPI() {
        shutdown();
    }

    ServerAPI& ServerAPI::instance() {
        static ServerAPI instance;
        return instance;
    }

    bool ServerAPI::init(const std::string& port) {
        try {
            Logger::logInfo("Initializing server on port %s", port.c_str());

            pImpl->server = std::make_unique<Server>(port);
            if (!pImpl->server->init()) {
                Logger::logError("Failed to initialize server");
                return false;
            }

            // Register routes
            Logger::logInfo("Registering routes");
            pImpl->server->addRoute(std::make_unique<ChatCompletionsRoute>());

            // Start server in a background thread
            std::thread([this]() {
                Logger::logInfo("Starting server main loop");
                pImpl->server->run();
                }).detach();

            return true;
        }
        catch (const std::exception& ex) {
            Logger::logError("Failed to initialize server: %s", ex.what());
            return false;
        }
    }

    void ServerAPI::shutdown() {
        if (pImpl->server) {
            Logger::logInfo("Shutting down server");
            // Implement server shutdown mechanism
            pImpl->server.reset();
        }
    }

    void ServerAPI::setInferenceCallback(InferenceCallback callback) {
        if (callback) {
            pImpl->inferenceCallback = std::move(callback);
            Logger::logInfo("Inference callback registered");
        }
        else {
            pImpl->inferenceCallback = defaultInference;
            Logger::logWarning("Null inference callback provided, using default");
        }
    }

    void ServerAPI::setStreamingInferenceCallback(StreamingInferenceCallback callback) {
        if (callback) {
            pImpl->streamingInferenceCallback = std::move(callback);
            Logger::logInfo("Streaming inference callback registered");
        }
        else {
            pImpl->streamingInferenceCallback = defaultStreamingInference;
            Logger::logWarning("Null streaming inference callback provided, using default");
        }
    }

    InferenceCallback ServerAPI::getInferenceCallback() const {
        return pImpl->inferenceCallback;
    }

    StreamingInferenceCallback ServerAPI::getStreamingInferenceCallback() const {
        return pImpl->streamingInferenceCallback;
    }

} // namespace kolosal
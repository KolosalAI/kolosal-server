#include "kolosal/server_api.hpp"
#include "kolosal/server.hpp"
#include "kolosal/routes/chat_completion_route.hpp"
#include "kolosal/routes/completion_route.hpp"
#include "kolosal/routes/models_route.hpp"
#include "kolosal/logger.hpp"
#include <memory>
#include <stdexcept>

namespace kolosal {

    // Default inference implementation (returns a simple not-implemented response)
    ChatCompletionResponse defaultChatCompletion(const ChatCompletionRequest& request) {
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
    bool defaultChatCompletionStreaming(const ChatCompletionRequest& request,
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

    // Default completion implementation (returns a simple not-implemented response)
    CompletionResponse defaultCompletion(const CompletionRequest& request) {
        CompletionResponse response;
        response.model = request.model;

        CompletionChoice choice;
        choice.index = 0;
        choice.text = "No completion implementation has been registered.";
        choice.finish_reason = "stop";

        response.choices.push_back(choice);

        // Simple token count estimation
        response.usage.prompt_tokens = 10;
        response.usage.completion_tokens = 10;
        response.usage.total_tokens = 20;

        return response;
    }

    // Default streaming completion (returns a simple not-implemented message in chunks)
    bool defaultCompletionStreaming(const CompletionRequest& request,
        const std::string& requestId,
        int chunkIndex,
        CompletionChunk& outputChunk) {
        outputChunk.id = requestId;
        outputChunk.model = request.model;

        CompletionChunkChoice choice;
        choice.index = 0;

        if (chunkIndex == 0) {
            // First chunk
            choice.text = "No streaming ";
            choice.finish_reason = "";
        }
        else if (chunkIndex == 1) {
            // Second chunk
            choice.text = "completion implementation ";
            choice.finish_reason = "";
        }
        else if (chunkIndex == 2) {
            // Third chunk
            choice.text = "has been registered.";
            choice.finish_reason = "stop";
        }

        outputChunk.choices.push_back(choice);

        // Return false when finished
        return chunkIndex < 2;
    }

    class ServerAPI::Impl {
    public:
        std::unique_ptr<Server> server;
        ChatCompletionCallback chatCompletionCallback;
        ChatCompletionStreamingCallback chatCompletionStreamingCallback;
        CompletionCallback completionCallback;
        CompletionStreamingCallback completionStreamingCallback;
		GetModelsCallback getModelsCallback;

        Impl()
            : chatCompletionCallback(defaultChatCompletion)
            , chatCompletionStreamingCallback(defaultChatCompletionStreaming)
            , completionCallback(defaultCompletion)
            , completionStreamingCallback(defaultCompletionStreaming)
			, getModelsCallback([]() { return std::vector<std::string>(); })
        {
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
            ServerLogger::logInfo("Initializing server on port %s", port.c_str());

            pImpl->server = std::make_unique<Server>(port);
            if (!pImpl->server->init()) {
                ServerLogger::logError("Failed to initialize server");
                return false;
            }

            // Register routes
            ServerLogger::logInfo("Registering routes");
            pImpl->server->addRoute(std::make_unique<ChatCompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<CompletionsRoute>());
			pImpl->server->addRoute(std::make_unique<ModelsRoute>());

            // Start server in a background thread
            std::thread([this]() {
                ServerLogger::logInfo("Starting server main loop");
                pImpl->server->run();
                }).detach();

            return true;
        }
        catch (const std::exception& ex) {
            ServerLogger::logError("Failed to initialize server: %s", ex.what());
            return false;
        }
    }

    void ServerAPI::shutdown() {
        if (pImpl->server) {
            ServerLogger::logInfo("Shutting down server");
            // Implement server shutdown mechanism
            pImpl->server.reset();
        }
    }

    void ServerAPI::setChatCompletionCallback(ChatCompletionCallback callback) {
        if (callback) {
            pImpl->chatCompletionCallback = std::move(callback);
            ServerLogger::logInfo("Chat completion callback registered");
        }
        else {
            pImpl->chatCompletionCallback = defaultChatCompletion;
            ServerLogger::logWarning("Null chat completion callback provided, using default");
        }
    }

    void ServerAPI::setChatCompletionStreamingCallback(ChatCompletionStreamingCallback callback) {
        if (callback) {
            pImpl->chatCompletionStreamingCallback = std::move(callback);
            ServerLogger::logInfo("Streaming chat completion callback registered");
        }
        else {
            pImpl->chatCompletionStreamingCallback = defaultChatCompletionStreaming;
            ServerLogger::logWarning("Null streaming chat completion callback provided, using default");
        }
    }

    ChatCompletionCallback ServerAPI::getChatCompletionCallback() const {
        return pImpl->chatCompletionCallback;
    }

    ChatCompletionStreamingCallback ServerAPI::getChatCompletionStreamingCallback() const {
        return pImpl->chatCompletionStreamingCallback;
    }

    void ServerAPI::setCompletionCallback(CompletionCallback callback) {
        if (callback) {
            pImpl->completionCallback = std::move(callback);
            ServerLogger::logInfo("Completion callback registered");
        }
        else {
            pImpl->completionCallback = defaultCompletion;
            ServerLogger::logWarning("Null completion callback provided, using default");
        }
    }

    void ServerAPI::setCompletionStreamingCallback(CompletionStreamingCallback callback) {
        if (callback) {
            pImpl->completionStreamingCallback = std::move(callback);
            ServerLogger::logInfo("Streaming completion callback registered");
        }
        else {
            pImpl->completionStreamingCallback = defaultCompletionStreaming;
            ServerLogger::logWarning("Null streaming completion callback provided, using default");
        }
    }

	void ServerAPI::setGetModelsCallback(GetModelsCallback callback) {
		if (callback) {
			pImpl->getModelsCallback = std::move(callback);
			ServerLogger::logInfo("Get models callback registered");
		}
		else {
			pImpl->getModelsCallback = []() { return std::vector<std::string>(); };
			ServerLogger::logWarning("Null get models callback provided, using default");
		}
	}

    CompletionCallback ServerAPI::getCompletionCallback() const {
        return pImpl->completionCallback;
    }

    CompletionStreamingCallback ServerAPI::getCompletionStreamingCallback() const {
        return pImpl->completionStreamingCallback;
    }

	GetModelsCallback ServerAPI::getGetModelsCallback() const {
		return pImpl->getModelsCallback;
	}

} // namespace kolosal
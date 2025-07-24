#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include <string>
#include <memory>

namespace kolosal {

    /**
     * @brief Combined route for handling inference completion requests using raw inference interface parameters
     * 
     * This route handles both:
     * - /v1/inference/completions (text completion)
     * - /v1/inference/chat/completions (chat completion)
     * 
     * Accepts ChatCompletionParameters or CompletionParameters directly and returns CompletionResult format,
     * providing a more direct interface to the inference engine without OpenAI API compatibility layers.
     */
    class KOLOSAL_SERVER_API CompletionRoute : public IRoute {
    public:
        CompletionRoute();
        ~CompletionRoute();
        
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
        
    private:
        // Specific handlers for different completion types
        void handleTextCompletion(SocketType sock, const std::string& body);
        void handleChatCompletion(SocketType sock, const std::string& body);
        
        // Path determination
        bool isTextCompletionPath(const std::string& path);
        bool isChatCompletionPath(const std::string& path);
    };

} // namespace kolosal

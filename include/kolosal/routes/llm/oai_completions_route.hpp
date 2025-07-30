#pragma once

#include "../../export.hpp"
#include "../route_interface.hpp"
#include "../../models/chat_request_model.hpp"
#include <string>
#include <memory>

namespace kolosal {

    /**
     * @brief Combined route for handling both OpenAI-compatible completion requests
     * 
     * This route handles both:
     * - /v1/completions (text completion)
     * - /v1/chat/completions (chat completion)
     * 
     * Provides OpenAI API compatibility for both completion types.
     */
    class KOLOSAL_SERVER_API OaiCompletionsRoute : public IRoute {
    public:
        OaiCompletionsRoute();
        ~OaiCompletionsRoute();
        
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
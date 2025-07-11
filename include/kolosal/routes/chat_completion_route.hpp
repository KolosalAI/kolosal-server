#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include "../models/chat_request_model.hpp"
#include <string>
#include <memory>

namespace kolosal {

    class KOLOSAL_SERVER_API ChatCompletionsRoute : public IRoute {
    public:
        ChatCompletionsRoute();
        ~ChatCompletionsRoute();
        
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;    private:
        // Helper methods
        int countTokens(const std::string& text);
    };

} // namespace kolosal
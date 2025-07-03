#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include <string>
#include <memory>

namespace kolosal {
    
    class CompletionMonitor;

    /**
     * @brief Route for handling inference chat completion requests using raw inference interface parameters
     * 
     * This route accepts ChatCompletionParameters directly and returns CompletionResult format,
     * providing a more direct interface to the inference engine without OpenAI API compatibility layers.
     */
    class KOLOSAL_SERVER_API InferenceChatCompletionRoute : public IRoute {
    public:
        InferenceChatCompletionRoute();
        ~InferenceChatCompletionRoute();
        
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
        
    private:
        CompletionMonitor* monitor_;  // Pointer to singleton instance
    };

} // namespace kolosal

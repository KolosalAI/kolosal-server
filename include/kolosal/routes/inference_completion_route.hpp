#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include <string>
#include <memory>

namespace kolosal {
    
    class CompletionMonitor;

    /**
     * @brief Route for handling inference completion requests using raw inference interface parameters
     * 
     * This route accepts CompletionParameters directly and returns CompletionResult format,
     * providing a more direct interface to the inference engine without OpenAI API compatibility layers.
     */
    class KOLOSAL_SERVER_API InferenceCompletionRoute : public IRoute {
    public:
        InferenceCompletionRoute();
        ~InferenceCompletionRoute();
        
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
        
    private:
        CompletionMonitor* monitor_;  // Pointer to singleton instance
    };

} // namespace kolosal

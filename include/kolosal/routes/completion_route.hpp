#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include <string>
#include <memory>

namespace kolosal {
    
    class CompletionMonitor;    class KOLOSAL_SERVER_API CompletionsRoute : public IRoute {
    public:
        CompletionsRoute();
        ~CompletionsRoute();
        
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
        
    private:
        CompletionMonitor* monitor_;  // Pointer to singleton instance
    };

} // namespace kolosal
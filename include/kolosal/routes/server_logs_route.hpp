#pragma once

#include "route_interface.hpp"
#include "../export.hpp"

#include <string>

namespace kolosal
{
    class KOLOSAL_SERVER_API ServerLogsRoute : public IRoute
    {
    public:
        bool match(const std::string &method, const std::string &path) override;
        void handle(SocketType sock, const std::string &body) override;
        
    private:
        // Store matched method for routing
        mutable std::string matched_method_;
        
        void handleOptions(SocketType sock);
    };
} // namespace kolosal

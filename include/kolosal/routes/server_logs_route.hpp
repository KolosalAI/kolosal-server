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
    };
} // namespace kolosal

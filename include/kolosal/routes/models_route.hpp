#pragma once

#include "../export.hpp"
#include "route_interface.hpp"
#include <string>

namespace kolosal {

    class KOLOSAL_SERVER_API ModelsRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    };

} // namespace kolosal
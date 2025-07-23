#pragma once

#include "kolosal/routes/route_interface.hpp"
#include "kolosal/server.hpp"
#include <string>

namespace kolosal::routes {

class SessionRoute : public IRoute {
public:
    SessionRoute();
    
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;
    void setup_routes(Server& server);
};

} // namespace kolosal::routes

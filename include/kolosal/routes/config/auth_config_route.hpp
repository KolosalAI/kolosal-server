#pragma once

#include "../route_interface.hpp"
#include "../../export.hpp"

namespace kolosal {

class KOLOSAL_SERVER_API AuthConfigRoute : public IRoute {
public:
    AuthConfigRoute();
    ~AuthConfigRoute() override = default;

    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;

private:
    // Store matched method and path for routing
    mutable std::string matched_method_;
    mutable std::string matched_path_;
    
    void handleGetConfig(SocketType sock);
    void handleUpdateConfig(SocketType sock, const std::string& body);
    void handleGetStats(SocketType sock);
    void handleClearRateLimit(SocketType sock, const std::string& body);
    void handleOptions(SocketType sock);
};

} // namespace kolosal

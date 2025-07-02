#pragma once

#include "kolosal/routes/route_interface.hpp"
#include <string>

namespace kolosal {

class CancelAllDownloadsRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override;
    void handle(SocketType sock, const std::string& body) override;
};

} // namespace kolosal

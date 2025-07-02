#ifndef KOLOSAL_DOWNLOADS_STATUS_ROUTE_HPP
#define KOLOSAL_DOWNLOADS_STATUS_ROUTE_HPP

#include "route_interface.hpp"

namespace kolosal {

    class DownloadsStatusRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    };

} // namespace kolosal

#endif // KOLOSAL_DOWNLOADS_STATUS_ROUTE_HPP

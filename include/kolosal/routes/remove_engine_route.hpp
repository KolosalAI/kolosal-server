#ifndef KOLOSAL_REMOVE_ENGINE_ROUTE_HPP
#define KOLOSAL_REMOVE_ENGINE_ROUTE_HPP

#include "route_interface.hpp"

namespace kolosal {

    class RemoveEngineRoute : public IRoute {
    public:
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    };

} // namespace kolosal

#endif // KOLOSAL_REMOVE_ENGINE_ROUTE_HPP

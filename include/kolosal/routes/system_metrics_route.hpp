#ifndef KOLOSAL_SYSTEM_METRICS_ROUTE_HPP
#define KOLOSAL_SYSTEM_METRICS_ROUTE_HPP

#include "route_interface.hpp"
#include "../system_monitor.hpp"
#include <memory>

namespace kolosal {

    class SystemMetricsRoute : public IRoute {
    public:
        SystemMetricsRoute();
        ~SystemMetricsRoute();

        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;

    private:
        std::unique_ptr<SystemMonitor> monitor_;
    };

} // namespace kolosal

#endif // KOLOSAL_SYSTEM_METRICS_ROUTE_HPP

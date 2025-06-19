#ifndef KOLOSAL_COMBINED_METRICS_ROUTE_HPP
#define KOLOSAL_COMBINED_METRICS_ROUTE_HPP

#include "route_interface.hpp"
#include "../system_monitor.hpp"
#include "../completion_monitor.hpp"
#include <memory>

namespace kolosal {

    /**
     * @brief Route for combined metrics endpoints that return both system and completion metrics
     * Handles /metrics and /v1/metrics endpoints
     */
    class CombinedMetricsRoute : public IRoute {
    public:
        CombinedMetricsRoute();
        ~CombinedMetricsRoute();

        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;

    private:
        std::unique_ptr<SystemMonitor> system_monitor_;
        CompletionMonitor* completion_monitor_;
    };

} // namespace kolosal

#endif // KOLOSAL_COMBINED_METRICS_ROUTE_HPP

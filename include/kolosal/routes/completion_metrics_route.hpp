#ifndef KOLOSAL_COMPLETION_METRICS_ROUTE_HPP
#define KOLOSAL_COMPLETION_METRICS_ROUTE_HPP

#include "kolosal/routes/route_interface.hpp"
#include "kolosal/completion_monitor.hpp"
#include "kolosal/export.hpp"
#include <memory>

namespace kolosal
{
    class KOLOSAL_SERVER_API CompletionMetricsRoute : public IRoute
    {
    public:
        CompletionMetricsRoute();
        ~CompletionMetricsRoute();

        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;    private:
        CompletionMonitor* monitor_;  // Pointer to singleton instance
    };

} // namespace kolosal

#endif // KOLOSAL_COMPLETION_METRICS_ROUTE_HPP

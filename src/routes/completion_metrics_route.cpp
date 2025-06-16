#include "kolosal/routes/completion_metrics_route.hpp"
#include "kolosal/models/completion_metrics_response_model.hpp"
#include "kolosal/metrics_converter.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace kolosal
{
    CompletionMetricsRoute::CompletionMetricsRoute()
        : monitor_(&CompletionMonitor::getInstance())
    {
        ServerLogger::logInfo("Completion Metrics Route initialized");
    }

    CompletionMetricsRoute::~CompletionMetricsRoute() = default;

    bool CompletionMetricsRoute::match(const std::string &method, const std::string &path)
    {
        return method == "GET" && (path == "/metrics/completion" ||
                                   path == "/v1/metrics/completion" ||
                                   path.find("/metrics/completion/") == 0 // For engine-specific metrics
                                   );
    }

    void CompletionMetricsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("Received completion metrics request");

            // Get completion metrics
            AggregatedCompletionMetrics metrics = monitor_->getCompletionMetrics();

            // Convert to DTO model
            CompletionMetricsResponseModel responseModel = utils::convertToCompletionMetricsResponse(metrics);

            // Create JSON response
            nlohmann::json response = responseModel.to_json();
            std::string responseStr = response.dump(2);

            send_response(sock, 200, responseStr);
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Error in completion metrics route: %s", e.what());
            std::string errorResponse = R"({"error": "Internal server error", "message": ")" + std::string(e.what()) + R"("})";
            send_response(sock, 500, errorResponse);
        }
    }

} // namespace kolosal

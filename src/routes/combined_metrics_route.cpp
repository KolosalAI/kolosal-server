#include "kolosal/routes/combined_metrics_route.hpp"
#include "kolosal/models/combined_metrics_response_model.hpp"
#include "kolosal/metrics_converter.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <iomanip>

using json = nlohmann::json;

namespace kolosal
{

    CombinedMetricsRoute::CombinedMetricsRoute()
        : system_monitor_(std::make_unique<SystemMonitor>()),
          completion_monitor_(&CompletionMonitor::getInstance())
    {
        ServerLogger::logInfo("CombinedMetricsRoute initialized");
    }

    CombinedMetricsRoute::~CombinedMetricsRoute() = default;

    bool CombinedMetricsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "GET" && (path == "/metrics" || path == "/v1/metrics"));
    }

    void CombinedMetricsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received combined metrics request", std::this_thread::get_id());

            // Get system metrics
            SystemMetrics systemMetrics = system_monitor_->getSystemMetrics();

            // Get completion metrics
            AggregatedCompletionMetrics completionMetrics = completion_monitor_->getCompletionMetrics();

            // Convert to combined DTO model
            CombinedMetricsResponseModel responseModel = utils::convertToCombinedMetricsResponse(
                systemMetrics, system_monitor_->isGPUMonitoringAvailable(), completionMetrics);

            // Create JSON response
            nlohmann::json response = responseModel.to_json();

            // Create HTTP response
            std::string responseStr = response.dump(2);
            std::string httpResponse = "HTTP/1.1 200 OK\r\n";
            httpResponse += "Content-Type: application/json\r\n";
            httpResponse += "Content-Length: " + std::to_string(responseStr.length()) + "\r\n";
            httpResponse += "Access-Control-Allow-Origin: *\r\n";
            httpResponse += "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
            httpResponse += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
            httpResponse += "\r\n";
            httpResponse += responseStr;

            send(sock, httpResponse.c_str(), static_cast<int>(httpResponse.length()), 0);

            ServerLogger::logInfo("[Thread %u] Combined metrics response sent successfully", std::this_thread::get_id());
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("[Thread %u] Error processing combined metrics request: %s",
                                   std::this_thread::get_id(), e.what());

            // Send error response
            json errorResponse = {
                {"error", {{"code", "internal_error"}, {"message", "Failed to retrieve combined metrics"}, {"details", e.what()}}},
                {"timestamp", SystemMonitor::getCurrentTimestamp()}};

            std::string responseStr = errorResponse.dump(2);
            std::string httpResponse = "HTTP/1.1 500 Internal Server Error\r\n";
            httpResponse += "Content-Type: application/json\r\n";
            httpResponse += "Content-Length: " + std::to_string(responseStr.length()) + "\r\n";
            httpResponse += "Access-Control-Allow-Origin: *\r\n";
            httpResponse += "\r\n";
            httpResponse += responseStr;

            send(sock, httpResponse.c_str(), static_cast<int>(httpResponse.length()), 0);
        }
    }

} // namespace kolosal

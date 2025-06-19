#include "kolosal/routes/system_metrics_route.hpp"
#include "kolosal/models/system_metrics_response_model.hpp"
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
    SystemMetricsRoute::SystemMetricsRoute() : monitor_(std::make_unique<SystemMonitor>())
    {
        ServerLogger::logInfo("SystemMetricsRoute initialized");
    }

    SystemMetricsRoute::~SystemMetricsRoute() = default;
    bool SystemMetricsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "GET" && (path == "/metrics/system" || path == "/v1/metrics/system"));
    }

    void SystemMetricsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received system metrics request", std::this_thread::get_id());

            // Get system metrics
            SystemMetrics metrics = monitor_->getSystemMetrics();
            // Convert to DTO model
            SystemMetricsResponseModel responseModel = utils::convertToSystemMetricsResponse(
                metrics, monitor_->isGPUMonitoringAvailable());

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

            ServerLogger::logInfo("[Thread %u] System metrics response sent successfully", std::this_thread::get_id());
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("[Thread %u] Error processing system metrics request: %s",
                                   std::this_thread::get_id(), e.what());

            // Send error response
            json errorResponse = {
                {"error", {{"code", "internal_error"}, {"message", "Failed to retrieve system metrics"}, {"details", e.what()}}},
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

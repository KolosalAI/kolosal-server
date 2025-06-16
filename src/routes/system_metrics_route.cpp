#include "kolosal/routes/system_metrics_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <iomanip>

using json = nlohmann::json;

namespace kolosal {

    SystemMetricsRoute::SystemMetricsRoute() : monitor_(std::make_unique<SystemMonitor>()) {
        ServerLogger::logInfo("SystemMetricsRoute initialized");
    }

    SystemMetricsRoute::~SystemMetricsRoute() = default;

    bool SystemMetricsRoute::match(const std::string& method, const std::string& path) {
        return (method == "GET" && (path == "/metrics" || path == "/v1/metrics" || path == "/system/metrics"));
    }

    void SystemMetricsRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received system metrics request", std::this_thread::get_id());

            // Get system metrics
            SystemMetrics metrics = monitor_->getSystemMetrics();            // Create JSON response
            json response = {
                {"timestamp", metrics.timestamp},
                {"cpu", {
                    {"usage_percent", metrics.cpuUsage >= 0 ? json(metrics.cpuUsage) : json()}
                }},
                {"memory", {
                    {"total_bytes", metrics.totalRAM},
                    {"used_bytes", metrics.usedRAM},
                    {"free_bytes", metrics.freeRAM},
                    {"utilization_percent", metrics.ramUtilization >= 0 ? json(metrics.ramUtilization) : json()},
                    {"total_formatted", SystemMonitor::formatBytes(metrics.totalRAM)},
                    {"used_formatted", SystemMonitor::formatBytes(metrics.usedRAM)},
                    {"free_formatted", SystemMonitor::formatBytes(metrics.freeRAM)}
                }},
                {"gpus", json::array()}
            };            // Add GPU information
            for (const auto& gpu : metrics.gpus) {
                json gpuJson = {
                    {"id", gpu.id},
                    {"name", gpu.name},
                    {"vendor", gpu.vendor},
                    {"utilization", {
                        {"gpu_percent", gpu.utilization >= 0 ? json(gpu.utilization) : json()},
                        {"memory_percent", gpu.memoryUtilization >= 0 ? json(gpu.memoryUtilization) : json()}
                    }},
                    {"memory", {
                        {"total_bytes", gpu.totalMemory},
                        {"used_bytes", gpu.usedMemory},
                        {"free_bytes", gpu.freeMemory},
                        {"total_formatted", SystemMonitor::formatBytes(gpu.totalMemory)},
                        {"used_formatted", SystemMonitor::formatBytes(gpu.usedMemory)},
                        {"free_formatted", SystemMonitor::formatBytes(gpu.freeMemory)}
                    }},
                    {"temperature_celsius", gpu.temperature >= 0 ? json(gpu.temperature) : json()},
                    {"power_usage_watts", gpu.powerUsage >= 0 ? json(gpu.powerUsage) : json()}
                };

                // Add driver version only for the first GPU to avoid redundancy
                if (gpu.id == 0 && !gpu.driverVersion.empty()) {
                    gpuJson["driver_version"] = gpu.driverVersion;
                }

                response["gpus"].push_back(gpuJson);
            }

            // Add GPU monitoring status
            response["gpu_monitoring_available"] = monitor_->isGPUMonitoringAvailable();

            // Create summary statistics
            double totalGpuUtilization = 0.0;
            double totalVramUtilization = 0.0;
            int validGpuReadings = 0;
            int validVramReadings = 0;

            for (const auto& gpu : metrics.gpus) {
                if (gpu.utilization >= 0) {
                    totalGpuUtilization += gpu.utilization;
                    validGpuReadings++;
                }
                if (gpu.memoryUtilization >= 0) {
                    totalVramUtilization += gpu.memoryUtilization;
                    validVramReadings++;
                }
            }            json summary = {
                {"cpu_usage_percent", metrics.cpuUsage >= 0 ? json(metrics.cpuUsage) : json()},
                {"ram_utilization_percent", metrics.ramUtilization >= 0 ? json(metrics.ramUtilization) : json()},
                {"gpu_count", static_cast<int>(metrics.gpus.size())},
                {"average_gpu_utilization_percent", validGpuReadings > 0 ? json(totalGpuUtilization / validGpuReadings) : json()},
                {"average_vram_utilization_percent", validVramReadings > 0 ? json(totalVramUtilization / validVramReadings) : json()}
            };

            response["summary"] = summary;

            // Add metadata
            response["metadata"] = {
                {"version", "1.0"},
                {"server", "kolosal-server"},
                {"monitoring_capabilities", {
                    {"cpu", true},
                    {"memory", true},
                    {"gpu", monitor_->isGPUMonitoringAvailable()}
                }}
            };

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

        } catch (const std::exception& e) {
            ServerLogger::logError("[Thread %u] Error processing system metrics request: %s", 
                                   std::this_thread::get_id(), e.what());

            // Send error response
            json errorResponse = {
                {"error", {
                    {"code", "internal_error"},
                    {"message", "Failed to retrieve system metrics"},
                    {"details", e.what()}
                }},
                {"timestamp", SystemMonitor::getCurrentTimestamp()}
            };

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

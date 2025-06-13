#include "kolosal/routes/health_status_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace kolosal {

    bool HealthStatusRoute::match(const std::string& method, const std::string& path) {
        return (method == "GET" && (path == "/health" || path == "/v1/health" || path == "/status"));
    }

    void HealthStatusRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received health status request", std::this_thread::get_id());

            // Get the NodeManager and collect metrics
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            auto engineIds = nodeManager.listEngineIds();

            // Count loaded vs unloaded engines
            int loadedCount = 0;
            int unloadedCount = 0;
            json engineSummary = json::array();

            for (const auto& engineId : engineIds) {
                auto engine = nodeManager.getEngine(engineId);
                if (engine) {
                    loadedCount++;
                } else {
                    unloadedCount++;
                }
                
                engineSummary.push_back({
                    {"engine_id", engineId},
                    {"status", engine ? "loaded" : "unloaded"}
                });
            }            // Get current timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto duration = now.time_since_epoch();
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            
            json response = {
                {"status", "healthy"},
                {"timestamp", millis},
                {"server", {
                    {"name", "Kolosal Inference Server"},
                    {"version", "1.0.0"},
                    {"uptime", "running"} // Could be enhanced with actual uptime
                }},
                {"node_manager", {
                    {"total_engines", engineIds.size()},
                    {"loaded_engines", loadedCount},
                    {"unloaded_engines", unloadedCount},
                    {"autoscaling", "enabled"}
                }},
                {"engines", engineSummary}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("[Thread %u] Successfully provided health status - %zu engines total (%d loaded, %d unloaded)", 
                                std::this_thread::get_id(), engineIds.size(), loadedCount, unloadedCount);

        }
        catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling health status request: %s", std::this_thread::get_id(), ex.what());

            json jError = { {"error", {
                {"message", std::string("Server error: ") + ex.what()},
                {"type", "server_error"},
                {"param", nullptr},
                {"code", nullptr}
            }} };

            send_response(sock, 500, jError.dump());
        }
    }

} // namespace kolosal

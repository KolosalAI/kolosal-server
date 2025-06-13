#include "kolosal/routes/list_engines_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>

using json = nlohmann::json;

namespace kolosal {

    bool ListEnginesRoute::match(const std::string& method, const std::string& path) {
        return (method == "GET" && (path == "/engines" || path == "/v1/engines"));
    }

    void ListEnginesRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received list engines request", std::this_thread::get_id());

            // Get the NodeManager and list engines
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            auto engineIds = nodeManager.listEngineIds();

            json enginesList = json::array();
            for (const auto& engineId : engineIds) {
                // Get engine to refresh activity and check status
                auto engine = nodeManager.getEngine(engineId);
                
                json engineInfo = {
                    {"engine_id", engineId},
                    {"status", engine ? "loaded" : "unloaded"},
                    {"last_accessed", "recently"} // Could be enhanced with actual timestamps
                };
                
                enginesList.push_back(engineInfo);
            }

            json response = {
                {"engines", enginesList},
                {"total_count", enginesList.size()}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("[Thread %u] Successfully listed %zu engines", std::this_thread::get_id(), enginesList.size());

        }
        catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling list engines request: %s", std::this_thread::get_id(), ex.what());

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

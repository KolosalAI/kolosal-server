#include "kolosal/routes/engine_status_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <regex>

using json = nlohmann::json;

namespace kolosal {

    bool EngineStatusRoute::match(const std::string& method, const std::string& path) {
        // Match GET /engines/{id}/status or GET /v1/engines/{id}/status
        std::regex statusPattern(R"(^/(v1/)?engines/([^/]+)/status$)");
        return (method == "GET" && std::regex_match(path, statusPattern));
    }

    void EngineStatusRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received engine status request", std::this_thread::get_id());

            // For now, we'll parse from body if provided since routing system doesn't pass path parameters
            // This is a workaround implementation
            json response = {
                {"error", {
                    {"message", "Engine status endpoint is available but requires path parameters that aren't currently accessible in this routing system. Use JSON body with engine_id for now."},
                    {"type", "not_implemented_error"},
                    {"param", nullptr},
                    {"code", nullptr}
                }}
            };

            // Check if we have engine_id in body as a workaround
            if (!body.empty()) {
                try {
                    auto j = json::parse(body);
                    if (j.contains("engine_id")) {
                        std::string engineId = j["engine_id"];
                        
                        // Get the NodeManager and check engine status
                        auto& nodeManager = ServerAPI::instance().getNodeManager();
                        auto engineIds = nodeManager.listEngineIds();
                        
                        // Check if engine exists
                        auto it = std::find(engineIds.begin(), engineIds.end(), engineId);
                        if (it != engineIds.end()) {
                            // Try to get the engine to check if it's loaded
                            auto engine = nodeManager.getEngine(engineId);
                            
                            response = {
                                {"engine_id", engineId},
                                {"status", engine ? "loaded" : "unloaded"},
                                {"available", true},
                                {"message", engine ? "Engine is loaded and ready" : "Engine exists but is currently unloaded"}
                            };

                            send_response(sock, 200, response.dump());
                            ServerLogger::logInfo("[Thread %u] Successfully retrieved status for engine '%s'", std::this_thread::get_id(), engineId.c_str());
                            return;
                        } else {
                            response = { {"error", {
                                {"message", "Engine not found"},
                                {"type", "not_found_error"},
                                {"param", "engine_id"},
                                {"code", nullptr}
                            }} };
                            
                            send_response(sock, 404, response.dump());
                            ServerLogger::logWarning("[Thread %u] Engine '%s' not found", std::this_thread::get_id(), engineId.c_str());
                            return;
                        }
                    }
                } catch (const json::exception&) {
                    // Fall through to default response
                }
            }

            send_response(sock, 501, response.dump());

        }
        catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling engine status request: %s", std::this_thread::get_id(), ex.what());

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

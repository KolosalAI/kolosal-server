#include "kolosal/routes/remove_engine_route.hpp"
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

    bool RemoveEngineRoute::match(const std::string& method, const std::string& path) {
        // Match DELETE /engines/{id} or DELETE /v1/engines/{id}
        std::regex enginePattern(R"(^/(v1/)?engines/([^/]+)$)");
        return (method == "DELETE" && std::regex_match(path, enginePattern));
    }

    void RemoveEngineRoute::handle(SocketType sock, const std::string& body) {
        try {
            ServerLogger::logInfo("[Thread %u] Received remove engine request", std::this_thread::get_id());

            // For now, we'll parse from body if provided, or indicate we need the path in the request
            // This is a temporary implementation - in a full server, the routing system would pass path parameters
            json response = {
                {"error", {
                    {"message", "Remove engine endpoint is available but requires path parameters that aren't currently accessible in this routing system. Use JSON body with engine_id for now."},
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
                        
                        // Get the NodeManager and remove the engine
                        auto& nodeManager = ServerAPI::instance().getNodeManager();
                        bool success = nodeManager.removeEngine(engineId);

                        if (success) {
                            response = {
                                {"engine_id", engineId},
                                {"status", "removed"},
                                {"message", "Engine removed successfully"}
                            };

                            send_response(sock, 200, response.dump());
                            ServerLogger::logInfo("[Thread %u] Successfully removed engine '%s'", std::this_thread::get_id(), engineId.c_str());
                            return;
                        } else {
                            response = { {"error", {
                                {"message", "Engine not found or could not be removed"},
                                {"type", "not_found_error"},
                                {"param", "engine_id"},
                                {"code", nullptr}
                            }} };
                            
                            send_response(sock, 404, response.dump());
                            ServerLogger::logWarning("[Thread %u] Failed to remove engine '%s' - not found", std::this_thread::get_id(), engineId.c_str());
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
            ServerLogger::logError("[Thread %u] Error handling remove engine request: %s", std::this_thread::get_id(), ex.what());

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

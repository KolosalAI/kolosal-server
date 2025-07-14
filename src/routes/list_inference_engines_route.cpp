#include "kolosal/routes/list_inference_engines_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>

using json = nlohmann::json;

namespace kolosal
{

    bool ListInferenceEnginesRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "GET" && (path == "/inference-engines" || path == "/v1/inference-engines"));
    }

    void ListInferenceEnginesRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received list inference engines request", std::this_thread::get_id());

            // Get the NodeManager and list available inference engines
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            auto availableEngines = nodeManager.getAvailableInferenceEngines();

            json enginesList = json::array();
            for (const auto &engine : availableEngines)
            {
                json engineInfo = {
                    {"name", engine.name},
                    {"version", engine.version},
                    {"description", engine.description},
                    {"library_path", engine.library_path},
                    {"is_loaded", engine.is_loaded}
                };

                enginesList.push_back(engineInfo);
            }

            json response = {
                {"inference_engines", enginesList},
                {"total_count", enginesList.size()}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logDebug("[Thread %u] Successfully listed %zu inference engines", std::this_thread::get_id(), enginesList.size());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling list inference engines request: %s", std::this_thread::get_id(), ex.what());

            json jError = {
                {"error", {
                    {"message", std::string("Server error: ") + ex.what()}, 
                    {"type", "server_error"}, 
                    {"param", nullptr}, 
                    {"code", nullptr}
                }}
            };

            send_response(sock, 500, jError.dump());
        }
    }

} // namespace kolosal

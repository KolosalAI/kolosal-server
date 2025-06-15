#include "kolosal/routes/engine_status_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/models/engine_status_request_model.hpp"
#include "kolosal/models/engine_status_response_model.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <regex>

using json = nlohmann::json;

namespace kolosal
{

    bool EngineStatusRoute::match(const std::string &method, const std::string &path)
    {
        // Match GET /engines/{id}/status or GET /v1/engines/{id}/status
        std::regex statusPattern(R"(^/(v1/)?engines/([^/]+)/status$)");
        return (method == "GET" && std::regex_match(path, statusPattern));
    }
    
    void EngineStatusRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received engine status request", std::this_thread::get_id());

            // For now, we'll parse from body if provided since routing system doesn't pass path parameters
            // This is a workaround implementation
            EngineStatusErrorResponse defaultErrorResponse;
            defaultErrorResponse.error.message = "Engine status endpoint is available but requires path parameters that aren't currently accessible in this routing system. Use JSON body with engine_id for now.";
            defaultErrorResponse.error.type = "not_implemented_error";
            defaultErrorResponse.error.param = "";
            defaultErrorResponse.error.code = "";

            // Check if we have engine_id in body as a workaround
            if (!body.empty())
            {
                try
                {
                    auto j = json::parse(body);

                    // Parse the request using the DTO model
                    EngineStatusRequest request;
                    request.from_json(j);

                    if (!request.validate())
                    {
                        EngineStatusErrorResponse errorResponse;
                        errorResponse.error.message = "Invalid request parameters";
                        errorResponse.error.type = "invalid_request_error";
                        errorResponse.error.param = "";
                        errorResponse.error.code = "";

                        send_response(sock, 400, errorResponse.to_json().dump());
                        return;
                    }

                    std::string engineId = request.engine_id;

                    // Get the NodeManager and check engine status
                    auto &nodeManager = ServerAPI::instance().getNodeManager();
                    auto engineIds = nodeManager.listEngineIds();

                    // Check if engine exists
                    auto it = std::find(engineIds.begin(), engineIds.end(), engineId);
                    if (it != engineIds.end())
                    {
                        // Try to get the engine to check if it's loaded
                        auto engine = nodeManager.getEngine(engineId);

                        EngineStatusResponse response;
                        response.engine_id = engineId;
                        response.status = engine ? "loaded" : "unloaded";
                        response.available = true;
                        response.message = engine ? "Engine is loaded and ready" : "Engine exists but is currently unloaded";

                        send_response(sock, 200, response.to_json().dump());
                        ServerLogger::logInfo("[Thread %u] Successfully retrieved status for engine '%s'", std::this_thread::get_id(), engineId.c_str());
                        return;
                    }
                    else
                    {
                        EngineStatusErrorResponse errorResponse;
                        errorResponse.error.message = "Engine not found";
                        errorResponse.error.type = "not_found_error";
                        errorResponse.error.param = "engine_id";
                        errorResponse.error.code = "";

                        send_response(sock, 404, errorResponse.to_json().dump());
                        ServerLogger::logWarning("[Thread %u] Engine '%s' not found", std::this_thread::get_id(), engineId.c_str());
                        return;
                    }
                }
                catch (const json::exception &ex)
                {
                    // Handle JSON parsing errors
                    EngineStatusErrorResponse errorResponse;
                    errorResponse.error.message = std::string("Invalid JSON: ") + ex.what();
                    errorResponse.error.type = "invalid_request_error";
                    errorResponse.error.param = "";
                    errorResponse.error.code = "";
                    send_response(sock, 400, errorResponse.to_json().dump());
                    ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
                    return;
                }
                catch (const std::runtime_error &ex)
                {
                    // Handle DTO model validation errors
                    EngineStatusErrorResponse errorResponse;
                    errorResponse.error.message = ex.what();
                    errorResponse.error.type = "invalid_request_error";
                    errorResponse.error.param = "";
                    errorResponse.error.code = "";

                    send_response(sock, 400, errorResponse.to_json().dump());
                    ServerLogger::logError("[Thread %u] Request validation error: %s", std::this_thread::get_id(), ex.what());
                    return;
                }
                catch (const std::exception &ex)
                {
                    // Handle any other exceptions during request processing
                    EngineStatusErrorResponse errorResponse;
                    errorResponse.error.message = std::string("Request processing error: ") + ex.what();
                    errorResponse.error.type = "server_error";
                    errorResponse.error.param = "";
                    errorResponse.error.code = "";

                    send_response(sock, 500, errorResponse.to_json().dump());
                    ServerLogger::logError("[Thread %u] Request processing error: %s", std::this_thread::get_id(), ex.what());
                    return;
                }
            }

            send_response(sock, 501, defaultErrorResponse.to_json().dump());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling engine status request: %s", std::this_thread::get_id(), ex.what());

            EngineStatusErrorResponse errorResponse;
            errorResponse.error.message = std::string("Server error: ") + ex.what();
            errorResponse.error.type = "server_error";
            errorResponse.error.param = "";
            errorResponse.error.code = "";

            send_response(sock, 500, errorResponse.to_json().dump());
        }
    }

} // namespace kolosal

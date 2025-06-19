#include "kolosal/routes/remove_engine_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/models/remove_engine_request_model.hpp"
#include "kolosal/models/remove_engine_response_model.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <regex>

using json = nlohmann::json;

namespace kolosal
{

    bool RemoveEngineRoute::match(const std::string &method, const std::string &path)
    {
        // Match DELETE /engines/{id} or DELETE /v1/engines/{id}
        std::regex enginePattern(R"(^/(v1/)?engines/([^/]+)$)");
        return (method == "DELETE" && std::regex_match(path, enginePattern));
    }

    void RemoveEngineRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received remove engine request", std::this_thread::get_id());

            // For now, we'll parse from body if provided since routing system doesn't pass path parameters
            // This is a workaround implementation
            RemoveEngineErrorResponse defaultErrorResponse;
            defaultErrorResponse.error.message = "Remove engine endpoint is available but requires path parameters that aren't currently accessible in this routing system. Use JSON body with engine_id for now.";
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
                    RemoveEngineRequest request;
                    request.from_json(j);

                    if (!request.validate())
                    {
                        RemoveEngineErrorResponse errorResponse;
                        errorResponse.error.message = "Invalid request parameters";
                        errorResponse.error.type = "invalid_request_error";
                        errorResponse.error.param = "";
                        errorResponse.error.code = "";

                        send_response(sock, 400, errorResponse.to_json().dump());
                        return;
                    }

                    std::string engineId = request.engine_id;

                    // Get the NodeManager and remove the engine
                    auto &nodeManager = ServerAPI::instance().getNodeManager();
                    bool success = nodeManager.removeEngine(engineId);

                    if (success)
                    {
                        RemoveEngineResponse response;
                        response.engine_id = engineId;
                        response.status = "removed";
                        response.message = "Engine removed successfully";

                        send_response(sock, 200, response.to_json().dump());
                        ServerLogger::logInfo("[Thread %u] Successfully removed engine '%s'", std::this_thread::get_id(), engineId.c_str());
                        return;
                    }
                    else
                    {
                        RemoveEngineErrorResponse errorResponse;
                        errorResponse.error.message = "Engine not found or could not be removed";
                        errorResponse.error.type = "not_found_error";
                        errorResponse.error.param = "engine_id";
                        errorResponse.error.code = "";

                        send_response(sock, 404, errorResponse.to_json().dump());
                        ServerLogger::logWarning("[Thread %u] Failed to remove engine '%s' - not found", std::this_thread::get_id(), engineId.c_str());
                        return;
                    }
                }
                catch (const json::exception &ex)
                {
                    // Handle JSON parsing errors
                    RemoveEngineErrorResponse errorResponse;
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
                    RemoveEngineErrorResponse errorResponse;
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
                    RemoveEngineErrorResponse errorResponse;
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
            ServerLogger::logError("[Thread %u] Error handling remove engine request: %s", std::this_thread::get_id(), ex.what());

            RemoveEngineErrorResponse errorResponse;
            errorResponse.error.message = std::string("Server error: ") + ex.what();
            errorResponse.error.type = "server_error";
            errorResponse.error.param = "";
            errorResponse.error.code = "";

            send_response(sock, 500, errorResponse.to_json().dump());
        }
    }

} // namespace kolosal

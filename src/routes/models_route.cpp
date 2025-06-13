#pragma once

#include "kolosal/routes/models_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace kolosal {

    bool ModelsRoute::match(const std::string& method, const std::string& path) {
		return (method == "GET" && (path == "/models" || path == "/v1/models"));
    }

    void ModelsRoute::handle(SocketType sock, const std::string& body) {
		try
		{
			auto getModelsCallback = ServerAPI::instance().getGetModelsCallback();

			if (getModelsCallback) {
				std::vector<std::string> models = getModelsCallback();
				json responseJson;
				responseJson["models"] = models;
				std::string responseString = responseJson.dump();
				send_response(sock, 200, responseString);
			}
			else {
				// Handle the case where the callback is not set
				json errorResponse;
				errorResponse["error"] = "No callback set for getting models.";
				std::string errorResponseString = errorResponse.dump();
				send_response(sock, 500, errorResponseString);
			}
		}
		catch (const json::exception& ex) {
			// Handle JSON parsing errors
			ServerLogger::logError("[Thread %u] JSON parsing error: %s",
				std::this_thread::get_id(), ex.what());
			json jError = { {"error", {
				{"message", std::string("Invalid JSON: ") + ex.what()},
				{"type", "invalid_request_error"},
				{"param", nullptr},
				{"code", nullptr}
			}} };
			send_response(sock, 400, jError.dump());
		}
		catch (const std::exception& e)
		{
			ServerLogger::logError("[Thread %u] Exception: %s",
				std::this_thread::get_id(), e.what());
			json jError = { {"error", {
				{"message", std::string("Exception: ") + e.what()},
				{"type", "internal_server_error"},
				{"param", nullptr},
				{"code", nullptr}
			}} };
			send_response(sock, 500, jError.dump());
		}
    }

} // namespace kolosal
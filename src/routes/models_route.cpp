#include "kolosal/routes/models_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/node_manager.h"
#include <json.hpp>
#include <vector>
#include <string>
#include <chrono>

using json = nlohmann::json;

namespace kolosal
{
    ModelsRoute::ModelsRoute()
    {
        ServerLogger::logInfo("ModelsRoute initialized");
    }

    ModelsRoute::~ModelsRoute() = default;

    bool ModelsRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "GET" && (path == "/v1/models" || path == "/models"));
    }

    void ModelsRoute::handle(SocketType sock, const std::string &body)
    {
        ServerLogger::logInfo("Processing GET /v1/models request");
        
        try
        {
            // Get the list of available models from NodeManager
            NodeManager* nodeManager = NodeManager::getInstance();
            std::vector<std::string> modelIds;
            
            if (nodeManager)
            {
                modelIds = nodeManager->getAvailableModels();
            }
            
            // Create OpenAI-compatible response
            json response;
            response["object"] = "list";
            response["data"] = json::array();
            
            // Get current timestamp
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            
            // Add each model to the response
            for (const auto& modelId : modelIds)
            {
                json model;
                model["id"] = modelId;
                model["object"] = "model";
                model["created"] = timestamp;
                model["owned_by"] = "kolosal";
                model["permission"] = json::array();
                model["root"] = modelId;
                model["parent"] = nullptr;
                
                response["data"].push_back(model);
            }
            
            // Add default model if none are available
            if (modelIds.empty())
            {
                json defaultModel;
                defaultModel["id"] = "kolosal-default";
                defaultModel["object"] = "model";
                defaultModel["created"] = timestamp;
                defaultModel["owned_by"] = "kolosal";
                defaultModel["permission"] = json::array();
                defaultModel["root"] = "kolosal-default";
                defaultModel["parent"] = nullptr;
                
                response["data"].push_back(defaultModel);
            }
            
            // Set OpenAI-compatible headers
            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"},
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, X-API-Key"}
            };
            
            send_response(sock, 200, response.dump(), headers);
            ServerLogger::logInfo("Models list sent successfully");
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Error in ModelsRoute: %s", ex.what());
            
            json jError = {
                {"error", {
                    {"message", std::string("Internal error: ") + ex.what()},
                    {"type", "server_error"},
                    {"param", nullptr},
                    {"code", nullptr}
                }}
            };
            
            std::map<std::string, std::string> headers = {
                {"Content-Type", "application/json"}
            };
            
            send_response(sock, 500, jError.dump(), headers);
        }
    }

} // namespace kolosal

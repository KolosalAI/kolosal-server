#include "kolosal/routes/engines_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

using json = nlohmann::json;

namespace kolosal
{

    bool EnginesRoute::match(const std::string &method, const std::string &path)
    {
        bool matches = ((method == "GET" || method == "POST" || method == "PUT") && (path == "/engines" || path == "/v1/engines"));
        
        // Store matched method for use in handle()
        if (matches)
        {
            matched_method_ = method;
        }
        
        return matches;
    }

    void EnginesRoute::handle(SocketType sock, const std::string &body)
    {
        // Route to appropriate handler based on method
        if (matched_method_ == "GET")
        {
            handleGetEngines(sock);
        }
        else if (matched_method_ == "POST")
        {
            handleAddEngine(sock, body);
        }
        else if (matched_method_ == "PUT")
        {
            handleSetDefaultEngine(sock, body);
        }
        else
        {
            json jError = {
                {"error", {
                    {"message", "Method not allowed. Use GET to list engines, POST to add engines, or PUT to set default engine."}, 
                    {"type", "method_not_allowed"}, 
                    {"param", nullptr}, 
                    {"code", nullptr}
                }}
            };
            send_response(sock, 405, jError.dump());
        }
    }

    void EnginesRoute::handleGetEngines(SocketType sock)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received list inference engines request", std::this_thread::get_id());

            // Get the NodeManager and list available inference engines
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            auto availableEngines = nodeManager.getAvailableInferenceEngines();

            // Get the default engine from config
            auto& config = ServerConfig::getInstance();
            std::string defaultEngine = config.defaultInferenceEngine;

            json enginesList = json::array();
            for (const auto &engine : availableEngines)
            {
                json engineInfo = {
                    {"name", engine.name},
                    {"version", engine.version},
                    {"description", engine.description},
                    {"library_path", engine.library_path},
                    {"is_loaded", engine.is_loaded},
                    {"is_default", engine.name == defaultEngine}
                };

                enginesList.push_back(engineInfo);
            }

            json response = {
                {"inference_engines", enginesList},
                {"default_engine", defaultEngine},
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

    void EnginesRoute::handleAddEngine(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received add inference engine request", std::this_thread::get_id());

            // Parse JSON request body
            json requestData;
            try
            {
                requestData = json::parse(body);
            }
            catch (const json::parse_error &e)
            {
                json jError = {
                    {"error", {
                        {"message", "Invalid JSON in request body"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "body"}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }

            // Validate required fields
            if (!requestData.contains("name") || !requestData.contains("library_path"))
            {
                json jError = {
                    {"error", {
                        {"message", "Missing required fields: 'name' and 'library_path' are required"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "body"}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }

            // Extract engine configuration
            std::string engineName = requestData["name"];
            std::string libraryPath = requestData["library_path"];
            std::string description = requestData.value("description", "");
            bool loadOnStartup = requestData.value("load_on_startup", true);

            // Validate engine name and path uniqueness
            auto& config = ServerConfig::getInstance();
            for (const auto& existingEngine : config.inferenceEngines)
            {
                if (existingEngine.name == engineName && existingEngine.library_path == libraryPath)
                {
                    // Engine with same name and path already exists in config
                    // Check actual loading status from NodeManager instead of assuming
                    auto& nodeManager = ServerAPI::instance().getNodeManager();
                    auto availableEngines = nodeManager.getAvailableInferenceEngines();
                    
                    bool actuallyLoaded = false;
                    for (const auto& engine : availableEngines)
                    {
                        if (engine.name == engineName && engine.library_path == libraryPath)
                        {
                            actuallyLoaded = engine.is_loaded;
                            break;
                        }
                    }
                    
                    json response = {
                        {"message", "Engine with name '" + engineName + "' and path '" + libraryPath + "' already exists"},
                        {"status", "success"},
                        {"engine", {
                            {"name", existingEngine.name},
                            {"library_path", existingEngine.library_path},
                            {"description", existingEngine.description},
                            {"load_on_startup", existingEngine.load_on_startup},
                            {"is_loaded", actuallyLoaded} // Use actual loading status
                        }}
                    };
                    send_response(sock, 200, response.dump());
                    ServerLogger::logInfo("[Thread %u] Engine '%s' already exists in config - actual load status: %s", 
                                        std::this_thread::get_id(), engineName.c_str(), actuallyLoaded ? "loaded" : "not loaded");
                    return;
                }
                else if (existingEngine.name == engineName)
                {
                    json jError = {
                        {"error", {
                            {"message", "Engine with name '" + engineName + "' already exists with different path"}, 
                            {"type", "invalid_request_error"}, 
                            {"param", "name"}, 
                            {"code", nullptr}
                        }}
                    };
                    send_response(sock, 409, jError.dump());
                    return;
                }
            }

            // Validate library path exists
            if (!std::filesystem::exists(libraryPath))
            {
                json jError = {
                    {"error", {
                        {"message", "Library file not found: " + libraryPath}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "library_path"}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }

            // Create new engine configuration
            InferenceEngineConfig newEngine(engineName, ServerConfig::makeAbsolutePath(libraryPath), description);
            newEngine.load_on_startup = loadOnStartup;

            // Add to server config
            config.inferenceEngines.push_back(newEngine);

            // Save updated config to file
            ServerLogger::logInfo("About to save configuration after adding engine '%s'", engineName.c_str());
            ServerLogger::logInfo("Current config file path in EnginesRoute: '%s'", config.getCurrentConfigFilePath().c_str());
            
            if (!config.saveToCurrentFile())
            {
                // Remove the engine from memory if save failed
                config.inferenceEngines.pop_back();
                
                json jError = {
                    {"error", {
                        {"message", "Failed to save configuration to file"}, 
                        {"type", "server_error"}, 
                        {"param", nullptr}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 500, jError.dump());
                return;
            }

            // Reconfigure inference loader with updated engines
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            
            if (!nodeManager.reconfigureEngines(config.inferenceEngines))
            {
                ServerLogger::logWarning("Failed to reconfigure inference engines after adding new engine");
            }

            // Prepare response
            json engineInfo = {
                {"name", newEngine.name},
                {"library_path", newEngine.library_path},
                {"description", newEngine.description},
                {"load_on_startup", newEngine.load_on_startup},
                {"is_loaded", false} // Newly added engines are not loaded by default
            };

            json response = {
                {"message", "Inference engine added successfully"},
                {"engine", engineInfo}
            };

            send_response(sock, 201, response.dump());
            ServerLogger::logInfo("[Thread %u] Successfully added inference engine: %s", std::this_thread::get_id(), engineName.c_str());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling add inference engine request: %s", std::this_thread::get_id(), ex.what());

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

    void EnginesRoute::handleSetDefaultEngine(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received set default inference engine request", std::this_thread::get_id());

            // Parse JSON request body
            json requestData;
            try
            {
                requestData = json::parse(body);
            }
            catch (const json::parse_error &e)
            {
                json jError = {
                    {"error", {
                        {"message", "Invalid JSON in request body"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "body"}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }

            // Validate required field
            if (!requestData.contains("engine_name"))
            {
                json jError = {
                    {"error", {
                        {"message", "Missing required field: 'engine_name' is required"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "body"}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 400, jError.dump());
                return;
            }

            std::string engineName = requestData["engine_name"];

            // Validate that the engine exists in the configuration
            auto& config = ServerConfig::getInstance();
            bool engineExists = false;
            for (const auto& engine : config.inferenceEngines)
            {
                if (engine.name == engineName)
                {
                    engineExists = true;
                    break;
                }
            }

            if (!engineExists)
            {
                json jError = {
                    {"error", {
                        {"message", "Engine '" + engineName + "' not found in configuration"}, 
                        {"type", "invalid_request_error"}, 
                        {"param", "engine_name"}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 404, jError.dump());
                return;
            }

            // Update the default engine in config
            config.defaultInferenceEngine = engineName;

            // Save updated config to file
            ServerLogger::logInfo("About to save configuration after setting default engine to '%s'", engineName.c_str());
            ServerLogger::logInfo("Current config file path in EnginesRoute: '%s'", config.getCurrentConfigFilePath().c_str());
            
            if (!config.saveToCurrentFile())
            {
                json jError = {
                    {"error", {
                        {"message", "Failed to save configuration to file"}, 
                        {"type", "server_error"}, 
                        {"param", nullptr}, 
                        {"code", nullptr}
                    }}
                };
                send_response(sock, 500, jError.dump());
                return;
            }

            // Prepare response
            json response = {
                {"message", "Default inference engine set successfully"},
                {"default_engine", engineName}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logInfo("[Thread %u] Successfully set default inference engine to: %s", std::this_thread::get_id(), engineName.c_str());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling set default inference engine request: %s", std::this_thread::get_id(), ex.what());

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

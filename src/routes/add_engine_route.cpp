#include "kolosal/routes/add_engine_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "inference_interface.h"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <filesystem>

using json = nlohmann::json;

namespace kolosal {

    bool AddEngineRoute::match(const std::string& method, const std::string& path) {
        return (method == "POST" && (path == "/engines" || path == "/v1/engines"));
    }

    void AddEngineRoute::handle(SocketType sock, const std::string& body) {
        try {
            // Check for empty body
            if (body.empty()) {
                throw std::invalid_argument("Request body is empty");
            }

            auto j = json::parse(body);
            ServerLogger::logInfo("[Thread %u] Received add engine request", std::this_thread::get_id());

            // Parse required fields
            if (!j.contains("engine_id") || !j.contains("model_path")) {
                json jError = { {"error", {
                    {"message", "Missing required fields: engine_id and model_path are required"},
                    {"type", "invalid_request_error"},
                    {"param", nullptr},
                    {"code", nullptr}
                }} };
                send_response(sock, 400, jError.dump());
                return;
            }

            std::string engineId = j["engine_id"];
            std::string modelPath = j["model_path"];
            bool loadImmediately = j.value("load_immediately", true);

            // Parse loading parameters
            LoadingParameters loadParams;
            if (j.contains("loading_parameters")) {
                auto loadParamsJson = j["loading_parameters"];
                loadParams.n_ctx = loadParamsJson.value("n_ctx", 4096);
                loadParams.n_keep = loadParamsJson.value("n_keep", 2048);
                loadParams.use_mlock = loadParamsJson.value("use_mlock", true);
                loadParams.use_mmap = loadParamsJson.value("use_mmap", true);
                loadParams.cont_batching = loadParamsJson.value("cont_batching", true);
                loadParams.warmup = loadParamsJson.value("warmup", false);
                loadParams.n_parallel = loadParamsJson.value("n_parallel", 1);
                loadParams.n_gpu_layers = loadParamsJson.value("n_gpu_layers", 100);
                loadParams.n_batch = loadParamsJson.value("n_batch", 2048);
                loadParams.n_ubatch = loadParamsJson.value("n_ubatch", 512);
            }

            int mainGpuId = j.value("main_gpu_id", 0);            // Validate model path before attempting to load
            std::string modelPathStr = modelPath;
            std::string errorMessage;
            std::string errorType = "server_error";
            int errorCode = 500;

            // Validate model path exists and is accessible
            if (!std::filesystem::exists(modelPathStr)) {
                errorMessage = "Model path '" + modelPathStr + "' does not exist. Please verify the path is correct.";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "model_path"},
                    {"code", "model_path_not_found"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Model path '%s' does not exist", std::this_thread::get_id(), modelPathStr.c_str());
                return;
            }

            // Check if path is a directory and contains model files
            if (std::filesystem::is_directory(modelPathStr)) {
                bool hasModelFile = false;
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(modelPathStr)) {
                        if (entry.path().extension() == ".gguf") {
                            hasModelFile = true;
                            break;
                        }
                    }
                } catch (const std::filesystem::filesystem_error& e) {
                    errorMessage = "Cannot access model directory '" + modelPathStr + "': " + e.what();
                    errorType = "invalid_request_error";
                    errorCode = 400;
                    
                    json jError = { {"error", {
                        {"message", errorMessage},
                        {"type", errorType},
                        {"param", "model_path"},
                        {"code", "model_path_access_denied"}
                    }} };
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Cannot access model directory '%s': %s", std::this_thread::get_id(), modelPathStr.c_str(), e.what());
                    return;
                }
                
                if (!hasModelFile) {
                    errorMessage = "No .gguf model files found in directory '" + modelPathStr + "'. Please ensure the directory contains a valid GGUF model file.";
                    errorType = "invalid_request_error";
                    errorCode = 400;
                    
                    json jError = { {"error", {
                        {"message", errorMessage},
                        {"type", errorType},
                        {"param", "model_path"},
                        {"code", "model_file_not_found"}
                    }} };
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] No .gguf files found in directory '%s'", std::this_thread::get_id(), modelPathStr.c_str());
                    return;
                }
            } else if (std::filesystem::is_regular_file(modelPathStr)) {
                // If it's a file, check if it's a .gguf file
                if (std::filesystem::path(modelPathStr).extension() != ".gguf") {
                    errorMessage = "Model file '" + modelPathStr + "' is not a .gguf file. Please provide a valid GGUF model file.";
                    errorType = "invalid_request_error";
                    errorCode = 400;
                    
                    json jError = { {"error", {
                        {"message", errorMessage},
                        {"type", errorType},
                        {"param", "model_path"},
                        {"code", "invalid_model_format"}
                    }} };
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Model file '%s' is not a .gguf file", std::this_thread::get_id(), modelPathStr.c_str());
                    return;
                }
            } else {
                errorMessage = "Model path '" + modelPathStr + "' is neither a file nor a directory. Please provide a valid path to a .gguf file or directory containing .gguf files.";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "model_path"},
                    {"code", "invalid_model_path_type"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Model path '%s' is not a valid file or directory", std::this_thread::get_id(), modelPathStr.c_str());
                return;
            }            // Validate loading parameters
            if (loadParams.n_ctx <= 0 || loadParams.n_ctx > 1000000) {
                errorMessage = "Invalid n_ctx value: " + std::to_string(loadParams.n_ctx) + ". Must be between 1 and 1,000,000.";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "loading_parameters.n_ctx"},
                    {"code", "invalid_parameter_value"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Invalid n_ctx value: %d", std::this_thread::get_id(), loadParams.n_ctx);
                return;
            }

            if (loadParams.n_keep < 0 || loadParams.n_keep > loadParams.n_ctx) {
                errorMessage = "Invalid n_keep value: " + std::to_string(loadParams.n_keep) + ". Must be between 0 and n_ctx (" + std::to_string(loadParams.n_ctx) + ").";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "loading_parameters.n_keep"},
                    {"code", "invalid_parameter_value"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Invalid n_keep value: %d (n_ctx: %d)", std::this_thread::get_id(), loadParams.n_keep, loadParams.n_ctx);
                return;
            }

            if (loadParams.n_batch <= 0 || loadParams.n_batch > 8192) {
                errorMessage = "Invalid n_batch value: " + std::to_string(loadParams.n_batch) + ". Must be between 1 and 8,192.";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "loading_parameters.n_batch"},
                    {"code", "invalid_parameter_value"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Invalid n_batch value: %d", std::this_thread::get_id(), loadParams.n_batch);
                return;
            }

            if (loadParams.n_ubatch <= 0 || loadParams.n_ubatch > loadParams.n_batch) {
                errorMessage = "Invalid n_ubatch value: " + std::to_string(loadParams.n_ubatch) + ". Must be between 1 and n_batch (" + std::to_string(loadParams.n_batch) + ").";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "loading_parameters.n_ubatch"},
                    {"code", "invalid_parameter_value"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Invalid n_ubatch value: %d (n_batch: %d)", std::this_thread::get_id(), loadParams.n_ubatch, loadParams.n_batch);
                return;
            }

            if (loadParams.n_parallel <= 0 || loadParams.n_parallel > 16) {
                errorMessage = "Invalid n_parallel value: " + std::to_string(loadParams.n_parallel) + ". Must be between 1 and 16.";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "loading_parameters.n_parallel"},
                    {"code", "invalid_parameter_value"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Invalid n_parallel value: %d", std::this_thread::get_id(), loadParams.n_parallel);
                return;
            }

            if (loadParams.n_gpu_layers < 0 || loadParams.n_gpu_layers > 1000) {
                errorMessage = "Invalid n_gpu_layers value: " + std::to_string(loadParams.n_gpu_layers) + ". Must be between 0 and 1,000.";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "loading_parameters.n_gpu_layers"},
                    {"code", "invalid_parameter_value"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Invalid n_gpu_layers value: %d", std::this_thread::get_id(), loadParams.n_gpu_layers);
                return;
            }            if (mainGpuId < -1 || mainGpuId > 15) {
                errorMessage = "Invalid main_gpu_id value: " + std::to_string(mainGpuId) + ". Must be between -1 (auto-select) and 15.";
                errorType = "invalid_request_error";
                errorCode = 400;
                
                json jError = { {"error", {
                    {"message", errorMessage},
                    {"type", errorType},
                    {"param", "main_gpu_id"},
                    {"code", "invalid_parameter_value"}
                }} };
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Invalid main_gpu_id value: %d", std::this_thread::get_id(), mainGpuId);
                return;
            }

            // Log configuration warnings for potentially problematic settings
            if (loadParams.n_ctx > 32768) {
                ServerLogger::logWarning("[Thread %u] Large context size (n_ctx=%d) may cause high memory usage for engine '%s'", 
                    std::this_thread::get_id(), loadParams.n_ctx, engineId.c_str());
            }

            if (loadParams.n_gpu_layers > 0 && mainGpuId == -1) {
                ServerLogger::logInfo("[Thread %u] GPU layers enabled but main_gpu_id is auto-select (-1) for engine '%s'", 
                    std::this_thread::get_id(), engineId.c_str());
            }

            if (loadParams.n_batch > 4096) {
                ServerLogger::logWarning("[Thread %u] Large batch size (n_batch=%d) may cause high memory usage for engine '%s'", 
                    std::this_thread::get_id(), loadParams.n_batch, engineId.c_str());
            }

            // Get the NodeManager and attempt to add the engine
            auto& nodeManager = ServerAPI::instance().getNodeManager();
            
            bool success = false;
            if (loadImmediately) {
                success = nodeManager.addEngine(engineId, modelPath.c_str(), loadParams, mainGpuId);
            } else {
                // For now, we'll still create the engine but we could extend NodeManager 
                // to support deferred loading in the future
                success = nodeManager.addEngine(engineId, modelPath.c_str(), loadParams, mainGpuId);
                ServerLogger::logInfo("Engine '%s' added with load_immediately=false (currently loading anyway)", engineId.c_str());
            }

            if (success) {
                json response = {
                    {"engine_id", engineId},
                    {"model_path", modelPath},
                    {"status", loadImmediately ? "loaded" : "created"},
                    {"load_immediately", loadImmediately},
                    {"loading_parameters", {
                        {"n_ctx", loadParams.n_ctx},
                        {"n_keep", loadParams.n_keep},
                        {"use_mlock", loadParams.use_mlock},
                        {"use_mmap", loadParams.use_mmap},
                        {"cont_batching", loadParams.cont_batching},
                        {"warmup", loadParams.warmup},
                        {"n_parallel", loadParams.n_parallel},
                        {"n_gpu_layers", loadParams.n_gpu_layers},
                        {"n_batch", loadParams.n_batch},
                        {"n_ubatch", loadParams.n_ubatch}
                    }},
                    {"main_gpu_id", mainGpuId},
                    {"message", "Engine added successfully"}
                };

                send_response(sock, 201, response.dump());
                ServerLogger::logInfo("[Thread %u] Successfully added engine '%s'", std::this_thread::get_id(), engineId.c_str());
            } else {
                // Model loading failed - check if it's a duplicate engine ID first
                auto existingEngineIds = nodeManager.listEngineIds();
                bool engineExists = false;
                for (const auto& existingId : existingEngineIds) {
                    if (existingId == engineId) {
                        engineExists = true;
                        break;
                    }
                }

                if (engineExists) {
                    errorMessage = "Engine ID '" + engineId + "' already exists. Please choose a different engine ID.";
                    errorType = "invalid_request_error";
                    errorCode = 409; // Conflict
                    
                    json jError = { {"error", {
                        {"message", errorMessage},
                        {"type", errorType},
                        {"param", "engine_id"},
                        {"code", "engine_id_exists"}
                    }} };
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Engine ID '%s' already exists", std::this_thread::get_id(), engineId.c_str());
                } else {
                    // Model loading failed - provide detailed error information
                    errorMessage = "Failed to load model from '" + modelPathStr + "'. ";
                    
                    // Try to determine the specific reason for failure
                    if (loadParams.n_gpu_layers > 0) {
                        errorMessage += "This could be due to: insufficient GPU memory, incompatible model format, corrupted model file, "
                                       "or GPU drivers not properly installed. Try reducing 'n_gpu_layers' or check the model file integrity.";
                        errorCode = 422; // Unprocessable Entity
                        errorType = "model_loading_error";
                    } else {
                        errorMessage += "This could be due to: insufficient system memory, corrupted model file, incompatible model format, "
                                       "or the model requiring more context than available. Try reducing 'n_ctx' or verify the model file.";
                        errorCode = 422; // Unprocessable Entity
                        errorType = "model_loading_error";
                    }

                    json jError = { {"error", {
                        {"message", errorMessage},
                        {"type", errorType},
                        {"param", "model_path"},
                        {"code", "model_loading_failed"},
                        {"details", {
                            {"engine_id", engineId},
                            {"model_path", modelPathStr},
                            {"n_ctx", loadParams.n_ctx},
                            {"n_gpu_layers", loadParams.n_gpu_layers},
                            {"main_gpu_id", mainGpuId}
                        }}
                    }} };
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Failed to load model for engine '%s' from path '%s'", std::this_thread::get_id(), engineId.c_str(), modelPathStr.c_str());
                }
            }
        }
        catch (const json::exception& ex) {
            // Specifically handle JSON parsing errors
            ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());

            json jError = { {"error", {
                {"message", std::string("Invalid JSON: ") + ex.what()},
                {"type", "invalid_request_error"},
                {"param", nullptr},
                {"code", nullptr}
            }} };

            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling add engine request: %s", std::this_thread::get_id(), ex.what());

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

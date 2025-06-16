#include "kolosal/routes/add_engine_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/models/add_engine_request_model.hpp"
#include "kolosal/download_utils.hpp"
#include "inference_interface.h"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <filesystem>

using json = nlohmann::json;

namespace kolosal
{

    bool AddEngineRoute::match(const std::string &method, const std::string &path)
    {
        return (method == "POST" && (path == "/engines" || path == "/v1/engines"));
    }

    void AddEngineRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            // Check for empty body
            if (body.empty())
            {
                throw std::invalid_argument("Request body is empty");
            }

            auto j = json::parse(body);
            ServerLogger::logInfo("[Thread %u] Received add engine request", std::this_thread::get_id());

            // Parse the request using the DTO model
            AddEngineRequest request;
            request.from_json(j);

            if (!request.validate())
            {
                json jError = {{"error", {{"message", "Invalid request parameters"}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
                send_response(sock, 400, jError.dump());
                return;
            }

            // Extract values from the model
            std::string engineId = request.engine_id;
            std::string modelPath = request.model_path;
            bool loadImmediately = request.load_immediately;
            int mainGpuId = request.main_gpu_id;

            // Convert model loading parameters to LoadingParameters struct
            LoadingParameters loadParams;
            loadParams.n_ctx = request.loading_parameters.n_ctx;
            loadParams.n_keep = request.loading_parameters.n_keep;
            loadParams.use_mlock = request.loading_parameters.use_mlock;
            loadParams.use_mmap = request.loading_parameters.use_mmap;
            loadParams.cont_batching = request.loading_parameters.cont_batching;
            loadParams.warmup = request.loading_parameters.warmup;
            loadParams.n_parallel = request.loading_parameters.n_parallel;
            loadParams.n_gpu_layers = request.loading_parameters.n_gpu_layers;
            loadParams.n_batch = request.loading_parameters.n_batch;            loadParams.n_ubatch = request.loading_parameters.n_ubatch; 
            
            // Validate model path before attempting to load
            std::string modelPathStr = modelPath;
            std::string errorMessage;
            std::string errorType = "server_error";
            int errorCode = 500;
            
            // Check if the model path is a URL
            bool isUrl = is_valid_url(modelPathStr);
            std::string actualModelPath = modelPathStr;
              if (isUrl) {
                ServerLogger::logInfo("[Thread %u] Model path is URL, starting download: %s", std::this_thread::get_id(), modelPathStr.c_str());
                
                // Generate download path - use ./models to match startup behavior
                std::string downloadPath = generate_download_path(modelPathStr, "./models");
                
                // Check if file already exists
                if (std::filesystem::exists(downloadPath)) {
                    ServerLogger::logInfo("[Thread %u] Model file already exists at: %s", std::this_thread::get_id(), downloadPath.c_str());
                    actualModelPath = downloadPath;
                } else {
                    // Download the file with progress callback
                    auto progressCallback = [&](size_t downloaded, size_t total, double percentage) {
                        ServerLogger::logInfo("[Thread %u] Download progress: %.1f%% (%zu/%zu bytes)", 
                                             std::this_thread::get_id(), percentage, downloaded, total);
                    };
                    
                    DownloadResult result = download_file(modelPathStr, downloadPath, progressCallback);
                    
                    if (!result.success) {
                        errorMessage = "Failed to download model from URL '" + modelPathStr + "': " + result.error_message;
                        errorType = "model_download_error";
                        errorCode = 422;
                        
                        json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "model_download_failed"}}}};
                        send_response(sock, errorCode, jError.dump());
                        ServerLogger::logError("[Thread %u] %s", std::this_thread::get_id(), errorMessage.c_str());
                        return;
                    }
                    
                    actualModelPath = result.local_path;
                    ServerLogger::logInfo("[Thread %u] Successfully downloaded model to: %s (%zu bytes)", 
                                         std::this_thread::get_id(), actualModelPath.c_str(), result.total_bytes);
                }
            }

            // Validate the actual model path exists and is accessible
            if (!std::filesystem::exists(actualModelPath)) {
                errorMessage = "Model path '" + actualModelPath + "' does not exist. Please verify the path is correct.";
                errorType = "invalid_request_error";
                errorCode = 400;

                json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "model_path_not_found"}}}};
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Model path '%s' does not exist", std::this_thread::get_id(), actualModelPath.c_str());
                return;
            }            // Check if path is a directory and contains model files
            if (std::filesystem::is_directory(actualModelPath))
            {
                bool hasModelFile = false;
                try
                {
                    for (const auto &entry : std::filesystem::directory_iterator(actualModelPath))
                    {
                        if (entry.path().extension() == ".gguf")
                        {
                            hasModelFile = true;
                            break;
                        }
                    }
                }
                catch (const std::filesystem::filesystem_error &e)
                {
                    errorMessage = "Cannot access model directory '" + actualModelPath + "': " + e.what();
                    errorType = "invalid_request_error";
                    errorCode = 400;

                    json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "model_path_access_denied"}}}};
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Cannot access model directory '%s': %s", std::this_thread::get_id(), actualModelPath.c_str(), e.what());
                    return;
                }

                if (!hasModelFile)
                {
                    errorMessage = "No .gguf model files found in directory '" + actualModelPath + "'. Please ensure the directory contains a valid GGUF model file.";
                    errorType = "invalid_request_error";
                    errorCode = 400;

                    json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "model_file_not_found"}}}};
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] No .gguf files found in directory '%s'", std::this_thread::get_id(), actualModelPath.c_str());
                    return;
                }
            }
            else if (std::filesystem::is_regular_file(actualModelPath))
            {
                // If it's a file, check if it's a .gguf file
                if (std::filesystem::path(actualModelPath).extension() != ".gguf")
                {
                    errorMessage = "Model file '" + actualModelPath + "' is not a .gguf file. Please provide a valid GGUF model file.";
                    errorType = "invalid_request_error";
                    errorCode = 400;

                    json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "invalid_model_format"}}}};
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Model file '%s' is not a .gguf file", std::this_thread::get_id(), actualModelPath.c_str());
                    return;
                }
            }
            else
            {
                errorMessage = "Model path '" + actualModelPath + "' is neither a file nor a directory. Please provide a valid path to a .gguf file or directory containing .gguf files.";
                errorType = "invalid_request_error";
                errorCode = 400;

                json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "invalid_model_path_type"}}}};
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Model path '%s' is not a valid file or directory", std::this_thread::get_id(), actualModelPath.c_str());
                return;
            }

            // Log configuration warnings for potentially problematic settings
            if (loadParams.n_ctx > 32768)
            {
                ServerLogger::logWarning("[Thread %u] Large context size (n_ctx=%d) may cause high memory usage for engine '%s'",
                                         std::this_thread::get_id(), loadParams.n_ctx, engineId.c_str());
            }

            if (loadParams.n_gpu_layers > 0 && mainGpuId == -1)
            {
                ServerLogger::logInfo("[Thread %u] GPU layers enabled but main_gpu_id is auto-select (-1) for engine '%s'",
                                      std::this_thread::get_id(), engineId.c_str());
            }

            if (loadParams.n_batch > 4096)
            {
                ServerLogger::logWarning("[Thread %u] Large batch size (n_batch=%d) may cause high memory usage for engine '%s'",
                                         std::this_thread::get_id(), loadParams.n_batch, engineId.c_str());
            }            // Get the NodeManager and attempt to add the engine
            auto &nodeManager = ServerAPI::instance().getNodeManager();            bool success = false;
            if (loadImmediately)
            {
                success = nodeManager.addEngine(engineId, actualModelPath.c_str(), loadParams, mainGpuId);
            }
            else
            {
                // Register the engine for lazy loading - model will be loaded on first access
                success = nodeManager.registerEngine(engineId, actualModelPath.c_str(), loadParams, mainGpuId);
                ServerLogger::logInfo("Engine '%s' registered with load_immediately=false (will load on first access)", engineId.c_str());
            }if (success)
            {
                json response = {
                    {"engine_id", engineId},
                    {"model_path", modelPath},
                    {"status", loadImmediately ? "loaded" : "created"},
                    {"load_immediately", loadImmediately},
                    {"loading_parameters", request.loading_parameters.to_json()},
                    {"main_gpu_id", mainGpuId},
                    {"message", "Engine added successfully"}};

                // Add additional info if model was downloaded from URL
                if (isUrl) {
                    response["download_info"] = {
                        {"source_url", modelPath},
                        {"local_path", actualModelPath},
                        {"was_downloaded", !std::filesystem::exists(actualModelPath) || modelPath != actualModelPath}
                    };
                }

                send_response(sock, 201, response.dump());
                ServerLogger::logInfo("[Thread %u] Successfully added engine '%s'", std::this_thread::get_id(), engineId.c_str());
            }
            else
            {
                // Model loading failed - check if it's a duplicate engine ID first
                auto existingEngineIds = nodeManager.listEngineIds();
                bool engineExists = false;
                for (const auto &existingId : existingEngineIds)
                {
                    if (existingId == engineId)
                    {
                        engineExists = true;
                        break;
                    }
                }

                if (engineExists)
                {
                    errorMessage = "Engine ID '" + engineId + "' already exists. Please choose a different engine ID.";
                    errorType = "invalid_request_error";
                    errorCode = 409; // Conflict

                    json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "engine_id"}, {"code", "engine_id_exists"}}}};
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Engine ID '%s' already exists", std::this_thread::get_id(), engineId.c_str());
                }                else
                {
                    // Model loading failed - provide detailed error information
                    errorMessage = "Failed to load model from '" + actualModelPath + "'. ";

                    // Try to determine the specific reason for failure
                    if (loadParams.n_gpu_layers > 0)
                    {
                        errorMessage += "This could be due to: insufficient GPU memory, incompatible model format, corrupted model file, "
                                        "or GPU drivers not properly installed. Try reducing 'n_gpu_layers' or check the model file integrity.";
                        errorCode = 422; // Unprocessable Entity
                        errorType = "model_loading_error";
                    }
                    else
                    {
                        errorMessage += "This could be due to: insufficient system memory, corrupted model file, incompatible model format, "
                                        "or the model requiring more context than available. Try reducing 'n_ctx' or verify the model file.";
                        errorCode = 422; // Unprocessable Entity
                        errorType = "model_loading_error";
                    }

                    json errorDetails = {
                        {"engine_id", engineId}, 
                        {"model_path", actualModelPath}, 
                        {"n_ctx", loadParams.n_ctx}, 
                        {"n_gpu_layers", loadParams.n_gpu_layers}, 
                        {"main_gpu_id", mainGpuId}
                    };
                    
                    // Add download info if applicable
                    if (isUrl) {
                        errorDetails["source_url"] = modelPath;
                        errorDetails["local_path"] = actualModelPath;
                    }

                    json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "model_loading_failed"}, {"details", errorDetails}}}};
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Failed to load model for engine '%s' from path '%s'", std::this_thread::get_id(), engineId.c_str(), actualModelPath.c_str());
                }
            }
        }
        catch (const json::exception &ex)
        {
            // Specifically handle JSON parsing errors
            ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Invalid JSON: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 400, jError.dump());
        }
        catch (const std::runtime_error &ex)
        {
            // Handle DTO model validation errors and other runtime errors
            ServerLogger::logError("[Thread %u] Request validation error: %s", std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling add engine request: %s", std::this_thread::get_id(), ex.what());

            json jError = {{"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};

            send_response(sock, 500, jError.dump());
        }
    }

} // namespace kolosal

#include "kolosal/routes/models_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/models/add_model_request_model.hpp"
#include "kolosal/models/add_model_response_model.hpp"
#include "kolosal/models/model_status_request_model.hpp"
#include "kolosal/models/model_status_response_model.hpp"
#include "kolosal/models/remove_model_request_model.hpp"
#include "kolosal/models/remove_model_response_model.hpp"
#include "kolosal/download_utils.hpp"
#include "kolosal/download_manager.hpp"
#include "inference_interface.h"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

using json = nlohmann::json;

namespace kolosal
{
    // Initialize static regex patterns
    const std::regex ModelsRoute::modelsPattern_(R"(^/(v1/)?models/?$)");
    const std::regex ModelsRoute::modelIdPattern_(R"(^/(v1/)?models/([^/]+)/?$)");
    const std::regex ModelsRoute::modelStatusPattern_(R"(^/(v1/)?models/([^/]+)/status/?$)");

    bool ModelsRoute::match(const std::string &method, const std::string &path)
    {
        // Match all model-related endpoints:
        // GET/POST /models, /v1/models
        // GET/DELETE /models/{id}, /v1/models/{id}  
        // GET /models/{id}/status, /v1/models/{id}/status
        
        bool matches = false;
        
        if (std::regex_match(path, modelsPattern_))
        {
            matches = (method == "GET" || method == "POST");
        }
        else if (std::regex_match(path, modelIdPattern_))
        {
            matches = (method == "GET" || method == "DELETE");
        }
        else if (std::regex_match(path, modelStatusPattern_))
        {
            matches = (method == "GET");
        }
        
        // Store matched path and method for use in handle()
        if (matches)
        {
            matched_path_ = path;
            matched_method_ = method;
        }
        
        return matches;
    }

    void ModelsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received %s request for path: %s", 
                                   std::this_thread::get_id(), matched_method_.c_str(), matched_path_.c_str());

            // Route to appropriate handler based on stored method and path
            if (std::regex_match(matched_path_, modelsPattern_))
            {
                if (matched_method_ == "GET")
                {
                    handleListModels(sock, body);
                }
                else if (matched_method_ == "POST")
                {
                    handleAddModel(sock, body);
                }
                else
                {
                    json jError = {{"error", {{"message", "Method not allowed"}, {"type", "method_not_allowed"}, {"param", nullptr}, {"code", nullptr}}}};
                    send_response(sock, 405, jError.dump());
                }
            }
            else if (std::regex_match(matched_path_, modelStatusPattern_))
            {
                if (matched_method_ == "GET")
                {
                    std::string modelId = extractModelIdFromPath(matched_path_);
                    handleModelStatus(sock, body, modelId);
                }
                else
                {
                    json jError = {{"error", {{"message", "Method not allowed"}, {"type", "method_not_allowed"}, {"param", nullptr}, {"code", nullptr}}}};
                    send_response(sock, 405, jError.dump());
                }
            }
            else if (std::regex_match(matched_path_, modelIdPattern_))
            {
                std::string modelId = extractModelIdFromPath(matched_path_);
                
                if (matched_method_ == "GET")
                {
                    handleGetModel(sock, body, modelId);
                }
                else if (matched_method_ == "DELETE")
                {
                    handleRemoveModel(sock, body, modelId);
                }
                else
                {
                    json jError = {{"error", {{"message", "Method not allowed"}, {"type", "method_not_allowed"}, {"param", nullptr}, {"code", nullptr}}}};
                    send_response(sock, 405, jError.dump());
                }
            }
            else
            {
                json jError = {{"error", {{"message", "Not found"}, {"type", "not_found"}, {"param", nullptr}, {"code", nullptr}}}};
                send_response(sock, 404, jError.dump());
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error in ModelsRoute: %s", std::this_thread::get_id(), ex.what());
            json jError = {{"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 500, jError.dump());
        }
    }

    void ModelsRoute::handleListModels(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logDebug("[Thread %u] Received list models request", std::this_thread::get_id());

            // Get the NodeManager and list engines
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            auto engineIds = nodeManager.listEngineIds();

            json modelsList = json::array();
            for (const auto &engineId : engineIds)
            {
                // Check engine status without loading it (to avoid triggering lazy model loading)
                auto [exists, isLoaded] = nodeManager.getEngineStatus(engineId);

                json modelInfo = {
                    {"model_id", engineId},
                    {"status", isLoaded ? "loaded" : "unloaded"},
                    {"last_accessed", "recently"} // Could be enhanced with actual timestamps
                };

                modelsList.push_back(modelInfo);
            }

            json response = {
                {"models", modelsList},
                {"total_count", modelsList.size()}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logDebug("[Thread %u] Successfully listed %zu models", std::this_thread::get_id(), modelsList.size());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling list models request: %s", std::this_thread::get_id(), ex.what());
            json jError = {{"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 500, jError.dump());
        }
    }

    void ModelsRoute::handleAddModel(SocketType sock, const std::string &body)
    {
        // This is a complex implementation, let's include the original logic from AddModelRoute
        try
        {
            ServerLogger::logInfo("[Thread %u] Received add model request", std::this_thread::get_id());

            if (body.empty())
            {
                json jError = {{"error", {{"message", "Request body is required"}, {"type", "invalid_request_error"}, {"param", "body"}, {"code", "missing_body"}}}};
                send_response(sock, 400, jError.dump());
                return;
            }

            auto j = json::parse(body);

            // Parse the request using the DTO model
            AddModelRequest request;
            request.from_json(j);

            if (!request.validate())
            {
                json jError = {{"error", {{"message", "Invalid request parameters"}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
                send_response(sock, 400, jError.dump());
                return;
            }

            std::string modelId = request.model_id;
            std::string modelPath = request.model_path;
            std::string inferenceEngine = request.inference_engine.empty() ? "llama-cpu" : request.inference_engine;
            int mainGpuId = request.main_gpu_id;
            bool loadImmediately = request.load_immediately;

            // Convert loading parameters
            LoadingParameters loadParams;
            loadParams.n_ctx = request.loading_parameters.n_ctx;
            loadParams.n_keep = request.loading_parameters.n_keep;
            loadParams.n_batch = request.loading_parameters.n_batch;
            loadParams.n_ubatch = request.loading_parameters.n_ubatch;
            loadParams.n_parallel = request.loading_parameters.n_parallel;
            loadParams.n_gpu_layers = request.loading_parameters.n_gpu_layers;
            loadParams.use_mmap = request.loading_parameters.use_mmap;
            loadParams.use_mlock = request.loading_parameters.use_mlock;
            loadParams.cont_batching = request.loading_parameters.cont_batching;
            loadParams.warmup = request.loading_parameters.warmup;

            std::string errorMessage;
            std::string errorType;
            int errorCode = 500;

            std::string modelPathStr = modelPath;
            std::string actualModelPath = modelPath;

            // Check if the model path is a URL
            bool isUrl = is_valid_url(modelPathStr);
            if (isUrl)
            {
                std::string downloadPath = "./models/" + std::filesystem::path(modelPathStr).filename().string();
                actualModelPath = downloadPath;

                if (std::filesystem::exists(downloadPath))
                {
                    // File exists locally, but we need to check if it's complete
                    auto &download_manager = DownloadManager::getInstance();
                    auto previous_download = download_manager.getDownloadProgress(modelId);
                    
                    bool fileIsComplete = false;
                    if (previous_download && previous_download->total_bytes > 0)
                    {
                        // Get actual file size
                        std::uintmax_t fileSize = std::filesystem::file_size(downloadPath);
                        
                        if (fileSize >= previous_download->total_bytes)
                        {
                            // File is complete
                            ServerLogger::logInfo("[Thread %u] Model file already complete locally at: %s (%llu bytes)", 
                                                  std::this_thread::get_id(), downloadPath.c_str(), fileSize);
                            fileIsComplete = true;
                        }
                        else
                        {
                            // File is incomplete - need to resume download
                            ServerLogger::logInfo("[Thread %u] Model file incomplete locally at: %s (%llu/%llu bytes), will resume download", 
                                                  std::this_thread::get_id(), downloadPath.c_str(), fileSize, previous_download->total_bytes);
                        }
                    }
                    else
                    {
                        // No previous download record - check if file seems reasonable in size
                        std::uintmax_t fileSize = std::filesystem::file_size(downloadPath);
                        // Assume files smaller than 100MB might be incomplete for large models
                        if (fileSize > 100 * 1024 * 1024) // 100 MB threshold
                        {
                            ServerLogger::logInfo("[Thread %u] Model file exists and seems complete at: %s (%llu bytes)", 
                                                  std::this_thread::get_id(), downloadPath.c_str(), fileSize);
                            fileIsComplete = true;
                        }
                        else
                        {
                            ServerLogger::logInfo("[Thread %u] Model file exists but seems small at: %s (%llu bytes), will restart download", 
                                                  std::this_thread::get_id(), downloadPath.c_str(), fileSize);
                        }
                    }
                    
                    if (fileIsComplete)
                    {
                        actualModelPath = downloadPath;
                    }
                    else
                    {
                        // File is incomplete, need to start/resume download
                        // Mark that we need to download by setting actualModelPath to empty
                        actualModelPath = "";
                    }
                }

                // Only start download if file doesn't exist or is incomplete
                if (!std::filesystem::exists(downloadPath) || actualModelPath.empty())
                {
                    // Start async download using DownloadManager with engine creation
                    auto &download_manager = DownloadManager::getInstance();

                    // Check if there's a previous download that was cancelled or failed
                    auto previous_download = download_manager.getDownloadProgress(modelId);
                    if (previous_download && (previous_download->cancelled || previous_download->status == "failed"))
                    {
                        ServerLogger::logInfo("[Thread %u] Previous download for model '%s' was cancelled or failed, restarting", 
                                             std::this_thread::get_id(), modelId.c_str());
                        // Cancel the old download record to clean it up
                        download_manager.cancelDownload(modelId);
                    }

                    // Prepare engine creation parameters
                    EngineCreationParams engine_params;
                    engine_params.model_id = modelId;
                    engine_params.load_immediately = loadImmediately;
                    engine_params.main_gpu_id = mainGpuId;
                    engine_params.loading_params = loadParams;
                    engine_params.inference_engine = inferenceEngine;

                    bool download_started = download_manager.startDownloadWithEngine(modelId, modelPathStr, downloadPath, engine_params);

                    if (!download_started)
                    {
                        // Check if download is already in progress
                        if (download_manager.isDownloadInProgress(modelId))
                        {
                            json jResponse = {
                                {"message", "Model download already in progress. Use /downloads/" + modelId + " to check status."},
                                {"model_id", modelId},
                                {"status", "downloading"},
                                {"download_url", modelPathStr},
                                {"local_path", downloadPath}
                            };
                            send_response(sock, 202, jResponse.dump());
                            ServerLogger::logInfo("[Thread %u] Model download already in progress: %s", std::this_thread::get_id(), modelId.c_str());
                            return;
                        }
                        else
                        {
                            json jError = {{"error", {{"message", "Failed to start download. This could be due to invalid URL or server configuration."}, {"type", "download_error"}, {"param", "model_path"}, {"code", "download_start_failed"}}}};
                            send_response(sock, 500, jError.dump());
                            ServerLogger::logError("[Thread %u] Failed to start download for model: %s", std::this_thread::get_id(), modelId.c_str());
                            return;
                        }
                    }

                    // Return 202 Accepted for async download
                    json jResponse = {
                        {"model_id", modelId},
                        {"status", "downloading"},
                        {"message", "Download started in background"},
                        {"download_url", modelPathStr},
                        {"local_path", downloadPath}
                    };

                    send_response(sock, 202, jResponse.dump());
                    ServerLogger::logInfo("[Thread %u] Started async download for model %s from URL: %s", std::this_thread::get_id(), modelId.c_str(), modelPathStr.c_str());
                    return;
                }
            }

            // Validate the actual model path exists and is accessible
            if (!std::filesystem::exists(actualModelPath))
            {
                errorMessage = "Model path '" + actualModelPath + "' does not exist. Please verify the path is correct.";
                errorType = "invalid_request_error";
                errorCode = 400;

                json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "model_path_not_found"}}}};
                send_response(sock, errorCode, jError.dump());
                ServerLogger::logError("[Thread %u] Model path '%s' does not exist", std::this_thread::get_id(), actualModelPath.c_str());
                return;
            }

            // Check if path is a directory and contains model files
            if (std::filesystem::is_directory(actualModelPath))
            {
                bool hasModelFile = false;
                try
                {
                    for (const auto &entry : std::filesystem::directory_iterator(actualModelPath))
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".gguf")
                        {
                            hasModelFile = true;
                            actualModelPath = entry.path().string(); // Use the first .gguf file found
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
                ServerLogger::logWarning("[Thread %u] Large context size (n_ctx=%d) may cause high memory usage for model '%s'",
                                         std::this_thread::get_id(), loadParams.n_ctx, modelId.c_str());
            }

            if (loadParams.n_gpu_layers > 0 && mainGpuId == -1)
            {
                ServerLogger::logInfo("[Thread %u] GPU layers enabled but main_gpu_id is auto-select (-1) for model '%s'",
                                      std::this_thread::get_id(), modelId.c_str());
            }

            if (loadParams.n_batch > 4096)
            {
                ServerLogger::logWarning("[Thread %u] Large batch size (n_batch=%d) may cause high memory usage for model '%s'",
                                         std::this_thread::get_id(), loadParams.n_batch, modelId.c_str());
            }

            // Get the NodeManager and attempt to add the engine
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            bool success = false;
            if (loadImmediately)
            {
                success = nodeManager.addEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId, inferenceEngine);
            }
            else
            {
                // Register the engine for lazy loading - model will be loaded on first access
                success = nodeManager.registerEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId, inferenceEngine);
                ServerLogger::logInfo("Model '%s' registered with load_immediately=false (will load on first access)", modelId.c_str());
            }

            if (success)
            {
                json response = {
                    {"model_id", modelId},
                    {"model_path", modelPath},
                    {"status", loadImmediately ? "loaded" : "created"},
                    {"load_immediately", loadImmediately},
                    {"loading_parameters", request.loading_parameters.to_json()},
                    {"main_gpu_id", mainGpuId},
                    {"message", "Engine added successfully"}
                };

                // Add additional info if model was downloaded from URL
                if (isUrl)
                {
                    response["download_info"] = {
                        {"source_url", modelPath},
                        {"local_path", actualModelPath},
                        {"was_downloaded", !std::filesystem::exists(actualModelPath) || modelPath != actualModelPath}
                    };
                }

                send_response(sock, 201, response.dump());
                ServerLogger::logInfo("[Thread %u] Successfully added model '%s'", std::this_thread::get_id(), modelId.c_str());
            }
            else
            {
                // Model loading failed - check if it's a duplicate model ID
                auto existingEngineIds = nodeManager.listEngineIds();
                bool engineExists = false;
                for (const auto &existingId : existingEngineIds)
                {
                    if (existingId == modelId)
                    {
                        engineExists = true;
                        break;
                    }
                }

                if (engineExists)
                {
                    // Check if the engine is actually loaded or just configured
                    auto [exists, isLoaded] = nodeManager.getEngineStatus(modelId);
                    
                    if (isLoaded)
                    {
                        // Model is already loaded and functional - return conflict
                        errorMessage = "Model ID '" + modelId + "' is already loaded and functional. Please choose a different model ID or remove the existing model first.";
                        errorType = "invalid_request_error";
                        errorCode = 409; // Conflict

                        json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_id"}, {"code", "model_already_loaded"}}}};
                        send_response(sock, errorCode, jError.dump());
                        ServerLogger::logError("[Thread %u] Model ID '%s' is already loaded", std::this_thread::get_id(), modelId.c_str());
                    }
                    else
                    {
                        // Model exists in config but is not loaded (could be from cancelled download or failed load)
                        // Try to remove it first, then retry adding
                        ServerLogger::logInfo("[Thread %u] Model '%s' exists but is not loaded, removing and retrying", std::this_thread::get_id(), modelId.c_str());
                        
                        bool removed = nodeManager.removeEngine(modelId);
                        if (removed)
                        {
                            // Retry adding the model
                            bool retrySuccess = false;
                            if (loadImmediately)
                            {
                                retrySuccess = nodeManager.addEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId, inferenceEngine);
                            }
                            else
                            {
                                retrySuccess = nodeManager.registerEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId, inferenceEngine);
                            }
                            
                            if (retrySuccess)
                            {
                                json response = {
                                    {"model_id", modelId},
                                    {"model_path", modelPath},
                                    {"status", loadImmediately ? "loaded" : "created"},
                                    {"load_immediately", loadImmediately},
                                    {"loading_parameters", request.loading_parameters.to_json()},
                                    {"main_gpu_id", mainGpuId},
                                    {"message", "Engine re-added successfully after removing previous failed configuration"}
                                };

                                // Add additional info if model was downloaded from URL
                                if (isUrl)
                                {
                                    response["download_info"] = {
                                        {"source_url", modelPath},
                                        {"local_path", actualModelPath},
                                        {"was_downloaded", !std::filesystem::exists(actualModelPath) || modelPath != actualModelPath}
                                    };
                                }

                                send_response(sock, 201, response.dump());
                                ServerLogger::logInfo("[Thread %u] Successfully re-added model '%s' after removing failed configuration", std::this_thread::get_id(), modelId.c_str());
                                return;
                            }
                        }
                        
                        // If we get here, the retry failed
                        errorMessage = "Model ID '" + modelId + "' exists but could not be removed or re-added. The previous configuration may be corrupted.";
                        errorType = "server_error";
                        errorCode = 500;

                        json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_id"}, {"code", "model_retry_failed"}}}};
                        send_response(sock, errorCode, jError.dump());
                        ServerLogger::logError("[Thread %u] Failed to retry adding model '%s' after removing failed configuration", std::this_thread::get_id(), modelId.c_str());
                    }
                }
                else
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
                        {"model_id", modelId},
                        {"model_path", actualModelPath},
                        {"n_ctx", loadParams.n_ctx},
                        {"n_gpu_layers", loadParams.n_gpu_layers},
                        {"main_gpu_id", mainGpuId}
                    };

                    // Add download info if applicable
                    if (isUrl)
                    {
                        errorDetails["source_url"] = modelPath;
                        errorDetails["local_path"] = actualModelPath;
                    }

                    json jError = {{"error", {{"message", errorMessage}, {"type", errorType}, {"param", "model_path"}, {"code", "model_loading_failed"}, {"details", errorDetails}}}};
                    send_response(sock, errorCode, jError.dump());
                    ServerLogger::logError("[Thread %u] Failed to load model for model '%s' from path '%s'", std::this_thread::get_id(), modelId.c_str(), actualModelPath.c_str());
                }
            }
        }
        catch (const json::exception &ex)
        {
            ServerLogger::logError("[Thread %u] JSON parsing error: %s", std::this_thread::get_id(), ex.what());
            json jError = {{"error", {{"message", std::string("Invalid JSON: ") + ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 400, jError.dump());
        }
        catch (const std::runtime_error &ex)
        {
            ServerLogger::logError("[Thread %u] Request validation error: %s", std::this_thread::get_id(), ex.what());
            json jError = {{"error", {{"message", ex.what()}, {"type", "invalid_request_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 400, jError.dump());
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling add model request: %s", std::this_thread::get_id(), ex.what());
            json jError = {{"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 500, jError.dump());
        }
    }

    void ModelsRoute::handleGetModel(SocketType sock, const std::string &body, const std::string &modelId)
    {
        // This is similar to handleModelStatus but with slightly different response format
        handleModelStatus(sock, body, modelId);
    }

    void ModelsRoute::handleRemoveModel(SocketType sock, const std::string &body, const std::string &modelId)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received remove model request for model: %s", std::this_thread::get_id(), modelId.c_str());

            // Get the NodeManager and remove the engine
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            bool success = nodeManager.removeEngine(modelId);

            if (success)
            {
                json response = {
                    {"model_id", modelId},
                    {"status", "removed"},
                    {"message", "Model removed successfully"}
                };

                send_response(sock, 200, response.dump());
                ServerLogger::logInfo("[Thread %u] Successfully removed model '%s'", std::this_thread::get_id(), modelId.c_str());
            }
            else
            {
                json errorResponse = {
                    {"error", {
                        {"message", "Model not found or could not be removed"},
                        {"type", "not_found_error"},
                        {"param", "model_id"},
                        {"code", "model_not_found"}
                    }}
                };

                send_response(sock, 404, errorResponse.dump());
                ServerLogger::logWarning("[Thread %u] Failed to remove model '%s' - not found", std::this_thread::get_id(), modelId.c_str());
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling remove model request: %s", std::this_thread::get_id(), ex.what());
            json jError = {{"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 500, jError.dump());
        }
    }

    void ModelsRoute::handleModelStatus(SocketType sock, const std::string &body, const std::string &modelId)
    {
        try
        {
            ServerLogger::logInfo("[Thread %u] Received model status request for model: %s", std::this_thread::get_id(), modelId.c_str());

            // Get the NodeManager and check engine status
            auto &nodeManager = ServerAPI::instance().getNodeManager();
            auto engineIds = nodeManager.listEngineIds();

            // Check if engine exists
            auto it = std::find(engineIds.begin(), engineIds.end(), modelId);
            if (it != engineIds.end())
            {
                // Check engine status without loading it (to avoid triggering lazy model loading)
                auto [exists, isLoaded] = nodeManager.getEngineStatus(modelId);

                json response = {
                    {"model_id", modelId},
                    {"status", isLoaded ? "loaded" : "unloaded"},
                    {"available", true},
                    {"message", isLoaded ? "Model is loaded and ready" : "Model exists but is currently unloaded"}
                };

                send_response(sock, 200, response.dump());
                ServerLogger::logInfo("[Thread %u] Successfully retrieved status for model '%s'", std::this_thread::get_id(), modelId.c_str());
            }
            else
            {
                json errorResponse = {
                    {"error", {
                        {"message", "Model not found"},
                        {"type", "not_found_error"},
                        {"param", "model_id"},
                        {"code", "model_not_found"}
                    }}
                };

                send_response(sock, 404, errorResponse.dump());
                ServerLogger::logWarning("[Thread %u] Model '%s' not found", std::this_thread::get_id(), modelId.c_str());
            }
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("[Thread %u] Error handling model status request: %s", std::this_thread::get_id(), ex.what());
            json jError = {{"error", {{"message", std::string("Server error: ") + ex.what()}, {"type", "server_error"}, {"param", nullptr}, {"code", nullptr}}}};
            send_response(sock, 500, jError.dump());
        }
    }

    std::string ModelsRoute::extractModelIdFromPath(const std::string &path)
    {
        std::smatch matches;
        
        // Try model status pattern first
        if (std::regex_match(path, matches, modelStatusPattern_))
        {
            return matches[2].str(); // Group 2 contains the model ID
        }
        
        // Try general model ID pattern
        if (std::regex_match(path, matches, modelIdPattern_))
        {
            return matches[2].str(); // Group 2 contains the model ID
        }
        
        return "";
    }

    bool ModelsRoute::isStatusEndpoint(const std::string &path)
    {
        return std::regex_match(path, modelStatusPattern_);
    }

} // namespace kolosal

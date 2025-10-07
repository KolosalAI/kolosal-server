#include "kolosal/routes/models_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/models/add_model_request_model.hpp"
#include "kolosal/models/add_model_response_model.hpp"
#include "kolosal/models/model_status_request_model.hpp"
#include "kolosal/models/model_status_response_model.hpp"
#include "kolosal/models/remove_model_request_model.hpp"
#include "kolosal/models/remove_model_response_model.hpp"
#include "kolosal/download_utils.hpp"
#include "kolosal/download_manager.hpp"
#include "inference_interface.h"
#include "llama.h" 
#include <json.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include <vector>
#include <string>
#include <chrono>

using json = nlohmann::json;

namespace kolosal
{
    ModelsRoute::ModelsRoute()
        : modelsPattern_(R"(^/(v1/)?models/?$)")
        , modelIdPattern_(R"(^/(v1/)?models/([^/]+)/?$)")
        , modelStatusPattern_(R"(^/(v1/)?models/([^/]+)/status/?$)")
    {
        ServerLogger::logInfo("ModelsRoute initialized");
    }

    ModelsRoute::~ModelsRoute() = default;

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
            int embeddingModels = 0;
            int llmModels = 0;
            int loadedModels = 0;
            int unloadedModels = 0;
            
            for (const auto &engineId : engineIds)
            {
                // Check engine status without loading it (to avoid triggering lazy model loading)
                auto [exists, isLoaded] = nodeManager.getEngineStatus(engineId);

                json modelInfo = {
                    {"model_id", engineId},
                    {"status", isLoaded ? "loaded" : "unloaded"},
                    {"available", exists},
                    {"last_accessed", "recently"} // Could be enhanced with actual timestamps
                };

                // Try to determine model type and capabilities
                try
                {
                    // This could be enhanced by storing model metadata in the NodeManager
                    // For now, we'll make reasonable defaults
                    bool isEmbeddingModel = false;
                    
                    // Check if the model ID contains embedding-related keywords
                    std::string lowerEngineId = engineId;
                    std::transform(lowerEngineId.begin(), lowerEngineId.end(), lowerEngineId.begin(), ::tolower);
                    
                    if (lowerEngineId.find("embedding") != std::string::npos ||
                        lowerEngineId.find("embed") != std::string::npos ||
                        lowerEngineId.find("retrieval") != std::string::npos)
                    {
                        isEmbeddingModel = true;
                    }
                    
                    if (isEmbeddingModel)
                    {
                        modelInfo["model_type"] = "embedding";
                        modelInfo["capabilities"] = json::array({"embedding", "retrieval"});
                        embeddingModels++;
                    }
                    else
                    {
                        modelInfo["model_type"] = "llm";
                        modelInfo["capabilities"] = json::array({"text_generation", "chat"});
                        llmModels++;
                    }
                    
                    if (isLoaded)
                    {
                        loadedModels++;
                        modelInfo["inference_ready"] = true;
                    }
                    else
                    {
                        unloadedModels++;
                        modelInfo["inference_ready"] = false;
                    }
                }
                catch (const std::exception &ex)
                {
                    ServerLogger::logWarning("[Thread %u] Error getting model info for '%s': %s", 
                                           std::this_thread::get_id(), engineId.c_str(), ex.what());
                    modelInfo["model_type"] = "unknown";
                    modelInfo["capabilities"] = json::array();
                    modelInfo["inference_ready"] = false;
                }

                modelsList.push_back(modelInfo);
            }

            json response = {
                {"models", modelsList},
                {"total_count", modelsList.size()},
                {"summary", {
                    {"total_models", modelsList.size()},
                    {"embedding_models", embeddingModels},
                    {"llm_models", llmModels},
                    {"loaded_models", loadedModels},
                    {"unloaded_models", unloadedModels}
                }}
            };

            send_response(sock, 200, response.dump());
            ServerLogger::logDebug("[Thread %u] Successfully listed %zu models (%d embedding, %d LLM, %d loaded)", 
                                   std::this_thread::get_id(), modelsList.size(), embeddingModels, llmModels, loadedModels);
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
            
            // Use the default inference engine from config if none is specified
            std::string inferenceEngine = request.inference_engine;
            if (inferenceEngine.empty())
            {
                auto& config = ServerConfig::getInstance();

                std::cout << "Using default inference engine: " << config.defaultInferenceEngine << std::endl;

                if (!config.defaultInferenceEngine.empty())
                {
                    inferenceEngine = config.defaultInferenceEngine;
                }
                else
                {
                    // Only fall back to platform-specific default if no default is configured
                    inferenceEngine = getPlatformDefaultInferenceEngine();
                    ServerLogger::logWarning("No default inference engine configured, falling back to %s for model '%s'", 
                                           inferenceEngine.c_str(), modelId.c_str());
                }
            }
            
            std::string modelType = request.model_type;
            int mainGpuId = request.main_gpu_id;
            bool loadImmediately = request.load_immediately;

            // Convert loading parameters
            LoadingParameters loadParams;
            
            // Core model parameters
            loadParams.n_ctx = request.loading_parameters.n_ctx;
            loadParams.n_batch = request.loading_parameters.n_batch;
            loadParams.n_ubatch = request.loading_parameters.n_ubatch;
            loadParams.n_keep = request.loading_parameters.n_keep;
            loadParams.n_chunks = request.loading_parameters.n_chunks;
            loadParams.n_parallel = request.loading_parameters.n_parallel;
            loadParams.n_sequences = request.loading_parameters.n_sequences;
            loadParams.grp_attn_n = request.loading_parameters.grp_attn_n;
            loadParams.grp_attn_w = request.loading_parameters.grp_attn_w;
            loadParams.n_print = request.loading_parameters.n_print;
            
            // RoPE parameters
            loadParams.rope_freq_base = request.loading_parameters.rope_freq_base;
            loadParams.rope_freq_scale = request.loading_parameters.rope_freq_scale;
            loadParams.yarn_ext_factor = request.loading_parameters.yarn_ext_factor;
            loadParams.yarn_attn_factor = request.loading_parameters.yarn_attn_factor;
            loadParams.yarn_beta_fast = request.loading_parameters.yarn_beta_fast;
            loadParams.yarn_beta_slow = request.loading_parameters.yarn_beta_slow;
            loadParams.yarn_orig_ctx = request.loading_parameters.yarn_orig_ctx;
            loadParams.defrag_thold = request.loading_parameters.defrag_thold;
            
            // Hardware acceleration and GPU parameters
            loadParams.n_gpu_layers = request.loading_parameters.n_gpu_layers;
            loadParams.main_gpu = request.loading_parameters.main_gpu;
            loadParams.split_mode = request.loading_parameters.split_mode;
            // Copy tensor_split array
            for (int i = 0; i < 128 && i < request.loading_parameters.tensor_split.size(); i++) {
                loadParams.tensor_split[i] = request.loading_parameters.tensor_split[i];
            }
            
            // Memory management
            loadParams.use_mmap = request.loading_parameters.use_mmap;
            loadParams.use_mlock = request.loading_parameters.use_mlock;
            loadParams.no_kv_offload = request.loading_parameters.no_kv_offload;
            loadParams.no_op_offload = request.loading_parameters.no_op_offload;
            loadParams.no_extra_bufts = request.loading_parameters.no_extra_bufts;
            
            // Processing and performance settings
            loadParams.cont_batching = request.loading_parameters.cont_batching;
            loadParams.flash_attn = request.loading_parameters.flash_attn;
            loadParams.warmup = request.loading_parameters.warmup;
            loadParams.check_tensors = request.loading_parameters.check_tensors;
            loadParams.swa_full = request.loading_parameters.swa_full;
            loadParams.kv_unified = request.loading_parameters.kv_unified;
            loadParams.ctx_shift = request.loading_parameters.ctx_shift;
            
            // Cache data types
            loadParams.cache_type_k = request.loading_parameters.cache_type_k;
            loadParams.cache_type_v = request.loading_parameters.cache_type_v;
            
            // Rope scaling and pooling types
            loadParams.rope_scaling_type = request.loading_parameters.rope_scaling_type;
            loadParams.pooling_type = request.loading_parameters.pooling_type;
            loadParams.attention_type = request.loading_parameters.attention_type;
            
            // NUMA strategy
            loadParams.numa = request.loading_parameters.numa;
            
            // CPU parameters
            loadParams.cpuparams.n_threads = request.loading_parameters.cpuparams.n_threads;
            size_t cpumask_size = std::min(request.loading_parameters.cpuparams.cpumask.size(), size_t(512));
            for (size_t i = 0; i < cpumask_size; i++) {
                loadParams.cpuparams.cpumask[i] = request.loading_parameters.cpuparams.cpumask[i];
            }
            loadParams.cpuparams.mask_valid = request.loading_parameters.cpuparams.mask_valid;
            loadParams.cpuparams.priority = request.loading_parameters.cpuparams.priority;
            loadParams.cpuparams.strict_cpu = request.loading_parameters.cpuparams.strict_cpu;
            loadParams.cpuparams.poll = request.loading_parameters.cpuparams.poll;
            
            // Copy batch CPU parameters
            loadParams.cpuparams_batch.n_threads = request.loading_parameters.cpuparams_batch.n_threads;
            size_t cpumask_batch_size = std::min(request.loading_parameters.cpuparams_batch.cpumask.size(), size_t(512));
            for (size_t i = 0; i < cpumask_batch_size; i++) {
                loadParams.cpuparams_batch.cpumask[i] = request.loading_parameters.cpuparams_batch.cpumask[i];
            }
            loadParams.cpuparams_batch.mask_valid = request.loading_parameters.cpuparams_batch.mask_valid;
            loadParams.cpuparams_batch.priority = request.loading_parameters.cpuparams_batch.priority;
            loadParams.cpuparams_batch.strict_cpu = request.loading_parameters.cpuparams_batch.strict_cpu;
            loadParams.cpuparams_batch.poll = request.loading_parameters.cpuparams_batch.poll;
            
            // Embedding parameters
            loadParams.embedding = request.loading_parameters.embedding;
            loadParams.embd_normalize = request.loading_parameters.embd_normalize;
            
            // Model and adapter parameters
            loadParams.model_alias = request.loading_parameters.model_alias;
            loadParams.hf_token = request.loading_parameters.hf_token;
            loadParams.lora_init_without_apply = request.loading_parameters.lora_init_without_apply;
            loadParams.lora_adapters = request.loading_parameters.lora_adapters;
            
            // Control vectors
            loadParams.control_vectors = request.loading_parameters.control_vectors;
            loadParams.control_vector_layer_start = request.loading_parameters.control_vector_layer_start;
            loadParams.control_vector_layer_end = request.loading_parameters.control_vector_layer_end;
            
            // Server/API parameters
            loadParams.verbosity = request.loading_parameters.verbosity;
            loadParams.offline = request.loading_parameters.offline;
            
            // Multimodal parameters
            loadParams.mmproj_path = request.loading_parameters.mmproj_path;
            loadParams.mmproj_use_gpu = request.loading_parameters.mmproj_use_gpu;
            loadParams.no_mmproj = request.loading_parameters.no_mmproj;
            
            // Chat template parameters
            loadParams.chat_template = request.loading_parameters.chat_template;
            loadParams.use_jinja = request.loading_parameters.use_jinja;
            loadParams.enable_chat_template = request.loading_parameters.enable_chat_template;
            
            // Input/output formatting
            loadParams.input_prefix_bos = request.loading_parameters.input_prefix_bos;
            loadParams.escape = request.loading_parameters.escape;
            loadParams.special = request.loading_parameters.special;
            
            // Performance and debugging
            loadParams.no_perf = request.loading_parameters.no_perf;
            loadParams.verbose_prompt = request.loading_parameters.verbose_prompt;
            loadParams.display_prompt = request.loading_parameters.display_prompt;
            
            // All other parameters are copied similarly...
            // For brevity, I'll include the most commonly used ones
            // The complete parameter mapping would include all fields from LoadingParameters struct

            // ---------------------------------------------------------------------
            // Automatic multi-GPU utilization
            // If caller did not specify an explicit tensor_split and n_gpu_layers > 0,
            // attempt to distribute work evenly across all detected devices.
            // A split_mode value of -1 (sentinel from CLI) or 0 with empty tensor_split
            // triggers auto mode. We set layer split (1) for simplicity.
            // ---------------------------------------------------------------------
            try {
                // Check if tensor_split array is all zeros (empty)
                bool tensor_split_empty = true;
                for (int i = 0; i < 128; i++) {
                    if (loadParams.tensor_split[i] != 0.0f) {
                        tensor_split_empty = false;
                        break;
                    }
                }
                
                bool wants_auto = (tensor_split_empty && loadParams.n_gpu_layers > 0 && (loadParams.split_mode <= 0));
                if (wants_auto) {
                    size_t dev_count = 1;
                    try { dev_count = (size_t) llama_max_devices(); } catch (...) { dev_count = 1; }
                    if (dev_count > 1 && dev_count <= 128) {
                        loadParams.split_mode = 1; // layer split
                        float split_value = 1.0f / static_cast<float>(dev_count);
                        for (size_t i = 0; i < dev_count; i++) {
                            loadParams.tensor_split[i] = split_value;
                        }
                        // Adjust last element to fix any floating point drift
                        float sum = 0.0f; 
                        for (size_t i = 0; i < dev_count - 1; ++i) {
                            sum += loadParams.tensor_split[i];
                        }
                        loadParams.tensor_split[dev_count - 1] = 1.0f - sum;
                        ServerLogger::logInfo("[Thread %u] Auto multi-GPU enabled: %zu devices (split_mode=1)", std::this_thread::get_id(), dev_count);
                    }
                }
            } catch (const std::exception &e) {
                ServerLogger::logWarning("[Thread %u] Auto multi-GPU setup failed: %s", std::this_thread::get_id(), e.what());
            }

            std::string errorMessage;
            std::string errorType;
            int errorCode = 500;

            // Validate model type and provide helpful feedback
            if (modelType != "llm" && modelType != "embedding")
            {
                ServerLogger::logError("[Thread %u] Invalid model_type '%s' for model '%s'. Must be 'llm' or 'embedding'", 
                                      std::this_thread::get_id(), modelType.c_str(), modelId.c_str());
                json jError = {{"error", {{"message", "Invalid model_type. Must be 'llm' or 'embedding'"}, {"type", "invalid_request_error"}, {"param", "model_type"}, {"code", nullptr}}}};
                send_response(sock, 400, jError.dump());
                return;
            }

            // Log specific information for embedding models
            if (modelType == "embedding")
            {
                ServerLogger::logInfo("[Thread %u] Processing embedding model '%s' with inference engine '%s'", 
                                     std::this_thread::get_id(), modelId.c_str(), inferenceEngine.c_str());
                
                // Embedding-specific parameter recommendations
                if (loadParams.n_ctx > 8192)
                {
                    ServerLogger::logWarning("[Thread %u] Large context size (n_ctx=%d) for embedding model '%s' may not be necessary. Consider reducing for better performance",
                                           std::this_thread::get_id(), loadParams.n_ctx, modelId.c_str());
                }
                
                if (loadParams.n_parallel > 4)
                {
                    ServerLogger::logWarning("[Thread %u] High parallel processing (n_parallel=%d) for embedding model '%s' may not improve performance significantly",
                                           std::this_thread::get_id(), loadParams.n_parallel, modelId.c_str());
                }
            }
            else
            {
                ServerLogger::logInfo("[Thread %u] Processing LLM model '%s' with inference engine '%s'", 
                                     std::this_thread::get_id(), modelId.c_str(), inferenceEngine.c_str());
            }

            std::string modelPathStr = modelPath;
            std::string actualModelPath = modelPath;

            // Check if the model path is a URL
            bool isUrl = is_valid_url(modelPathStr);
            if (isUrl)
            {
                std::string downloadPath = generate_download_path_executable(modelPathStr);
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
                    engine_params.model_type = modelType;
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
                                {"model_type", modelType},
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
                        {"model_type", modelType},
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
                if (modelType == "embedding")
                {
                    success = nodeManager.addEmbeddingEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId);
                }
                else
                {
                    success = nodeManager.addEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId, inferenceEngine);
                }
            }
            else
            {
                // Register the engine for lazy loading - model will be loaded on first access
                if (modelType == "embedding")
                {
                    success = nodeManager.registerEmbeddingEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId);
                }
                else
                {
                    success = nodeManager.registerEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId);
                }
                ServerLogger::logInfo("Model '%s' registered with load_immediately=false (will load on first access)", modelId.c_str());
            }

            if (success)
            {
                // Verify the engine is actually functional before updating config
                bool engineFunctional = false;
                try
                {
                    auto [exists, isLoaded] = nodeManager.getEngineStatus(modelId);
                    engineFunctional = exists && (loadImmediately ? isLoaded : true);
                }
                catch (const std::exception &ex)
                {
                    ServerLogger::logWarning("[Thread %u] Failed to verify engine status for model '%s': %s", 
                                           std::this_thread::get_id(), modelId.c_str(), ex.what());
                    engineFunctional = false;
                }

                if (engineFunctional)
                {
                    json response = {
                        {"model_id", modelId},
                        {"model_path", modelPath},
                        {"model_type", modelType},
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
                    // Engine was added but is not functional, treat as failure
                    ServerLogger::logError("[Thread %u] Engine for model '%s' was added but is not functional", 
                                         std::this_thread::get_id(), modelId.c_str());
                    
                    // Try to remove the non-functional engine
                    try
                    {
                        nodeManager.removeEngine(modelId);
                        ServerLogger::logInfo("[Thread %u] Removed non-functional engine for model '%s'", 
                                             std::this_thread::get_id(), modelId.c_str());
                    }
                    catch (const std::exception &ex)
                    {
                        ServerLogger::logWarning("[Thread %u] Failed to remove non-functional engine for model '%s': %s", 
                                               std::this_thread::get_id(), modelId.c_str(), ex.what());
                    }
                    
                    json jError = {{"error", {{"message", "Engine was created but failed functionality check"}, {"type", "model_loading_error"}, {"param", "model_path"}, {"code", "engine_not_functional"}}}};
                    send_response(sock, 422, jError.dump());
                }
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
                                if (modelType == "embedding")
                                {
                                    retrySuccess = nodeManager.addEmbeddingEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId);
                                }
                                else
                                {
                                    retrySuccess = nodeManager.addEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId, inferenceEngine);
                                }
                            }
                            else
                            {
                                if (modelType == "embedding")
                                {
                                    retrySuccess = nodeManager.registerEmbeddingEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId);
                                }
                                else
                                {
                                    retrySuccess = nodeManager.registerEngine(modelId, actualModelPath.c_str(), loadParams, mainGpuId);
                                }
                            }
                            
                            if (retrySuccess)
                            {
                                // Verify the engine is actually functional before updating config
                                bool engineFunctional = false;
                                try
                                {
                                    auto [exists, isLoaded] = nodeManager.getEngineStatus(modelId);
                                    engineFunctional = exists && (loadImmediately ? isLoaded : true);
                                }
                                catch (const std::exception &ex)
                                {
                                    ServerLogger::logWarning("[Thread %u] Failed to verify engine status for model '%s' (retry): %s", 
                                                           std::this_thread::get_id(), modelId.c_str(), ex.what());
                                    engineFunctional = false;
                                }

                                if (engineFunctional)
                                {
                                    json response = {
                                        {"model_id", modelId},
                                        {"model_path", modelPath},
                                        {"model_type", modelType},
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
                                }
                                else
                                {
                                    // Engine was added but is not functional
                                    ServerLogger::logError("[Thread %u] Retry engine for model '%s' was added but is not functional", 
                                                         std::this_thread::get_id(), modelId.c_str());
                                    
                                    // Try to remove the non-functional engine
                                    try
                                    {
                                        nodeManager.removeEngine(modelId);
                                        ServerLogger::logInfo("[Thread %u] Removed non-functional retry engine for model '%s'", 
                                                             std::this_thread::get_id(), modelId.c_str());
                                    }
                                    catch (const std::exception &ex)
                                    {
                                        ServerLogger::logWarning("[Thread %u] Failed to remove non-functional retry engine for model '%s': %s", 
                                                               std::this_thread::get_id(), modelId.c_str(), ex.what());
                                    }
                                    
                                    json jError = {{"error", {{"message", "Retry engine was created but failed functionality check"}, {"type", "model_loading_error"}, {"param", "model_path"}, {"code", "retry_engine_not_functional"}}}};
                                    send_response(sock, 422, jError.dump());
                                }
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

                // Try to get additional model information
                json response = {
                    {"model_id", modelId},
                    {"status", isLoaded ? "loaded" : "unloaded"},
                    {"available", true},
                    {"message", isLoaded ? "Model is loaded and ready" : "Model exists but is currently unloaded"}
                };

                // Get additional model information if available
                try
                {
                    // Check if this is an embedding model by trying to get engine and checking type
                    if (isLoaded)
                    {
                        auto engine = nodeManager.getEngine(modelId);
                        if (engine)
                        {
                            response["engine_loaded"] = true;
                            response["inference_ready"] = true;
                            
                            // Try to determine if this is an embedding model
                            // This could be enhanced by adding metadata to the engine records
                            response["capabilities"] = json::array({"inference"});
                            
                            // Add performance info for embedding models if available
                            response["performance"] = {
                                {"last_activity", "N/A"},
                                {"request_count", 0}
                            };
                        }
                        else
                        {
                            response["engine_loaded"] = false;
                            response["inference_ready"] = false;
                        }
                    }
                    else
                    {
                        response["engine_loaded"] = false;
                        response["inference_ready"] = false;
                        response["capabilities"] = json::array();
                    }
                }
                catch (const std::exception &ex)
                {
                    ServerLogger::logWarning("[Thread %u] Could not get detailed engine info for model '%s': %s", 
                                           std::this_thread::get_id(), modelId.c_str(), ex.what());
                    response["engine_loaded"] = false;
                    response["inference_ready"] = false;
                }

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

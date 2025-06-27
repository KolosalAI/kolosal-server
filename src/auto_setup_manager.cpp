#include "kolosal/auto_setup_manager.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/agents/multi_agent_system.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/download_utils.hpp"
#include "inference_interface.h"
#include <json.hpp>
#include <filesystem>
#include <fstream>
#include <thread>

using json = nlohmann::json;

namespace kolosal {

AutoSetupManager::AutoSetupManager(
    std::shared_ptr<NodeManager> node_manager,
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager
) : node_manager_(node_manager), agent_manager_(agent_manager) {
    setup_default_engine_configs();
    ServerLogger::logInfo("AutoSetupManager initialized");
}

bool AutoSetupManager::perform_auto_setup() {
    ServerLogger::logInfo("Starting automatic server setup...");
    
    bool success = true;
    
    // 1. Auto-setup engines (download models if needed)
    if (!auto_setup_engines()) {
        ServerLogger::logWarning("Engine auto-setup failed, but continuing...");
        success = false;
    }
    
    // 2. Auto-discover agents
    if (!auto_discover_agents()) {
        ServerLogger::logWarning("Agent auto-discovery failed, but continuing...");
        success = false;
    }
    
    // 3. Validate setup
    if (is_default_engine_ready() && are_agents_available()) {
        ServerLogger::logInfo("✅ Auto-setup completed successfully!");
        ServerLogger::logInfo("   - Default engine: READY");
        ServerLogger::logInfo("   - Available agents: %d", static_cast<int>(get_available_agent_names().size()));
        return true;
    } else {
        ServerLogger::logWarning("⚠️  Auto-setup completed with issues:");
        ServerLogger::logInfo("   - Default engine: %s", is_default_engine_ready() ? "READY" : "NOT READY");
        ServerLogger::logInfo("   - Available agents: %d", static_cast<int>(get_available_agent_names().size()));
        return success;
    }
}

bool AutoSetupManager::auto_setup_engines() {
    ServerLogger::logInfo("Setting up engines automatically...");
    
    // Check if default engine already exists
    if (ensure_default_engine_exists()) {
        ServerLogger::logInfo("Default engine already exists and is ready");
        default_engine_ready_ = true;
        return true;
    }
    
    // Try to setup default engines from configuration
    bool any_success = false;
    for (const auto& config : default_engine_configs_) {
        ServerLogger::logInfo("Attempting to setup engine: %s", config.engine_id.c_str());
        
        if (create_engine_from_config(config)) {
            ServerLogger::logInfo("✅ Successfully setup engine: %s", config.engine_id.c_str());
            if (config.engine_id == "default") {
                default_engine_ready_ = true;
            }
            any_success = true;
        } else {
            ServerLogger::logWarning("❌ Failed to setup engine: %s", config.engine_id.c_str());
        }
    }
    
    return any_success;
}

bool AutoSetupManager::ensure_default_engine_exists() {
    // Check if default engine is already loaded
    auto engine = node_manager_->getEngine("default");
    if (engine) {
        ServerLogger::logInfo("Default engine already exists");
        return true;
    }
    
    // Check if any engine exists that could serve as default
    auto engine_ids = node_manager_->listEngineIds();
    if (!engine_ids.empty()) {
        ServerLogger::logInfo("Found existing engine, treating as default: %s", engine_ids[0].c_str());
        return true;
    }
    
    return false;
}

bool AutoSetupManager::create_engine_from_config(const EngineConfig& config) {
    // Check if model file exists, download if needed
    if (!download_model_if_missing(config)) {
        return false;
    }
    
    // Setup loading parameters
    LoadingParameters load_params;
    load_params.n_ctx = config.n_ctx;
    load_params.n_gpu_layers = config.n_gpu_layers;
    load_params.n_batch = config.n_batch;
    load_params.use_mlock = false;
    load_params.use_mmap = true;
    load_params.cont_batching = false;
    load_params.warmup = true;
    load_params.n_parallel = 1;
    load_params.n_keep = 0;
    load_params.n_ubatch = 512;
    
    // Add engine to node manager
    bool success = node_manager_->addEngine(
        config.engine_id,
        config.model_path.c_str(),
        load_params,
        config.main_gpu_id
    );
    
    if (success) {
        ServerLogger::logInfo("Engine '%s' created successfully", config.engine_id.c_str());
    } else {
        ServerLogger::logError("Failed to create engine '%s'", config.engine_id.c_str());
    }
    
    return success;
}

bool AutoSetupManager::download_model_if_missing(const EngineConfig& config) {
    // Check if model file already exists
    if (std::filesystem::exists(config.model_path)) {
        if (validate_model_file(config.model_path)) {
            ServerLogger::logInfo("Model file already exists: %s", config.model_path.c_str());
            return true;
        } else {
            ServerLogger::logWarning("Model file exists but appears invalid: %s", config.model_path.c_str());
        }
    }
    
    // Skip download if auto-download is disabled
    if (!auto_download_enabled_ || config.download_url.empty()) {
        ServerLogger::logWarning("Auto-download disabled or no URL provided for: %s", config.model_path.c_str());
        return false;
    }
    
    // Create download directory if it doesn't exist
    std::filesystem::path model_path(config.model_path);
    std::filesystem::create_directories(model_path.parent_path());
    
    ServerLogger::logInfo("Downloading model from: %s", config.download_url.c_str());
    ServerLogger::logInfo("Download target: %s", config.model_path.c_str());
    
    // Download with progress callback
    auto progress_callback = [&](size_t downloaded, size_t total, double percentage) {
        if (static_cast<int>(percentage) % 10 == 0) { // Log every 10%
            ServerLogger::logInfo("Download progress: %.1f%% (%zu/%zu bytes)", 
                                percentage, downloaded, total);
        }
    };
    
    DownloadResult result = download_file(config.download_url, config.model_path, progress_callback);
    
    if (result.success) {
        ServerLogger::logInfo("✅ Model downloaded successfully: %s", config.model_path.c_str());
        return validate_model_file(config.model_path);
    } else {
        ServerLogger::logError("❌ Model download failed: %s", result.error_message.c_str());
        return false;
    }
}

bool AutoSetupManager::auto_discover_agents() {
    ServerLogger::logInfo("Auto-discovering agents...");
    
    refresh_agent_cache();
    
    if (agent_name_to_uuid_.empty()) {
        ServerLogger::logWarning("No agents discovered");
        return false;
    }
    
    ServerLogger::logInfo("Discovered %d agents:", static_cast<int>(agent_name_to_uuid_.size()));
    for (const auto& [name, uuid] : agent_name_to_uuid_) {
        ServerLogger::logInfo("  - %s -> %s", name.c_str(), uuid.c_str());
    }
    
    agents_cached_ = true;
    return true;
}

void AutoSetupManager::refresh_agent_cache() {
    agent_name_to_uuid_.clear();
    
    if (!agent_manager_) {
        ServerLogger::logWarning("Agent manager not available");
        return;
    }
    
    try {
        // Get all agents from the agent manager
        std::vector<std::string> agent_ids = agent_manager_->list_agents();
        
        for (const std::string& agent_id : agent_ids) {
            auto agent_core = agent_manager_->get_agent(agent_id);
            if (agent_core) {
                std::string name = agent_core->get_agent_name();
                std::string uuid = agent_core->get_agent_id();
                
                if (!name.empty() && !uuid.empty()) {
                    agent_name_to_uuid_[name] = uuid;
                    ServerLogger::logDebug("Cached agent mapping: %s -> %s", name.c_str(), uuid.c_str());
                }
            }
        }
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Failed to refresh agent cache: %s", e.what());
    }
}

std::unordered_map<std::string, std::string> AutoSetupManager::get_agent_name_to_uuid_mapping() {
    if (!agents_cached_) {
        refresh_agent_cache();
    }
    return agent_name_to_uuid_;
}

bool AutoSetupManager::auto_register_workflow_with_agent_mapping(const std::string& workflow_json) {
    try {
        // Parse workflow JSON
        json workflow = json::parse(workflow_json);
        
        // Map agent names to UUIDs in the workflow
        std::string mapped_workflow = map_agent_names_to_uuids_in_workflow(workflow_json);
        
        if (mapped_workflow.empty()) {
            ServerLogger::logError("Failed to map agent names to UUIDs in workflow");
            return false;
        }
        
        // The actual workflow registration would be handled by the SequentialWorkflowRoute
        // This function prepares the workflow for registration
        ServerLogger::logInfo("Workflow prepared with agent UUID mapping");
        return true;
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Failed to auto-register workflow: %s", e.what());
        return false;
    }
}

std::string AutoSetupManager::map_agent_names_to_uuids_in_workflow(const std::string& workflow_json) {
    try {
        json workflow = json::parse(workflow_json);
        
        // Ensure agent cache is fresh
        if (!agents_cached_) {
            refresh_agent_cache();
        }
        
        // Map agent names to UUIDs in workflow steps
        if (workflow.contains("steps") && workflow["steps"].is_array()) {
            for (auto& step : workflow["steps"]) {
                if (step.contains("agent_id") && step["agent_id"].is_string()) {
                    std::string agent_name = step["agent_id"];
                    
                    auto it = agent_name_to_uuid_.find(agent_name);
                    if (it != agent_name_to_uuid_.end()) {
                        step["agent_id"] = it->second;
                        ServerLogger::logDebug("Mapped agent '%s' to UUID '%s' in workflow step", 
                                             agent_name.c_str(), it->second.c_str());
                    } else {
                        ServerLogger::logWarning("Agent '%s' not found in mapping", agent_name.c_str());
                        return ""; // Return empty string to indicate failure
                    }
                }
            }
        }
        
        return workflow.dump();
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Failed to map agent names in workflow: %s", e.what());
        return "";
    }
}

bool AutoSetupManager::is_default_engine_ready() {
    if (default_engine_ready_) {
        return true;
    }
    
    // Check if default engine exists now
    auto engine = node_manager_->getEngine("default");
    if (engine) {
        default_engine_ready_ = true;
        return true;
    }
    
    return false;
}

bool AutoSetupManager::are_agents_available() {
    if (!agents_cached_) {
        refresh_agent_cache();
    }
    return !agent_name_to_uuid_.empty();
}

std::vector<std::string> AutoSetupManager::get_available_agent_names() {
    if (!agents_cached_) {
        refresh_agent_cache();
    }
    
    std::vector<std::string> names;
    names.reserve(agent_name_to_uuid_.size());
    
    for (const auto& [name, uuid] : agent_name_to_uuid_) {
        names.push_back(name);
    }
    
    return names;
}

std::string AutoSetupManager::get_setup_status_json() {
    json status;
    
    status["auto_setup_enabled"] = true;
    status["default_engine_ready"] = is_default_engine_ready();
    status["agents_available"] = are_agents_available();
    status["agent_count"] = static_cast<int>(get_available_agent_names().size());
    status["auto_download_enabled"] = auto_download_enabled_;
    status["download_directory"] = download_directory_;
    
    // Engine status
    auto engine_ids = node_manager_->listEngineIds();
    status["engines"] = engine_ids;
    status["engine_count"] = static_cast<int>(engine_ids.size());
    
    // Agent mappings
    json agent_mappings = json::object();
    for (const auto& [name, uuid] : get_agent_name_to_uuid_mapping()) {
        agent_mappings[name] = uuid;
    }
    status["agent_mappings"] = agent_mappings;
    
    return status.dump(2);
}

void AutoSetupManager::setup_default_engine_configs() {
    // Default Qwen model configuration
    EngineConfig qwen_config;
    qwen_config.engine_id = "default";
    qwen_config.model_path = "downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf";
    qwen_config.download_url = "https://huggingface.co/QuantFactory/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/Qwen2.5-0.5B-Instruct.Q4_K_M.gguf";
    qwen_config.n_ctx = 4096;
    qwen_config.n_gpu_layers = 0;
    qwen_config.n_batch = 512;
    qwen_config.n_threads = 4;
    qwen_config.auto_download = true;
    
    default_engine_configs_.push_back(qwen_config);
    
    ServerLogger::logInfo("Default engine configurations setup complete");
}

bool AutoSetupManager::validate_model_file(const std::string& path) {
    try {
        // Basic file existence and size check
        if (!std::filesystem::exists(path)) {
            return false;
        }
        
        auto file_size = std::filesystem::file_size(path);
        if (file_size < 1024 * 1024) { // Less than 1MB is probably not a valid model
            ServerLogger::logWarning("Model file suspiciously small: %zu bytes", file_size);
            return false;
        }
        
        // TODO: Add more sophisticated model validation (magic numbers, etc.)
        return true;
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Error validating model file: %s", e.what());
        return false;
    }
}

void AutoSetupManager::add_default_engine_config(const EngineConfig& config) {
    default_engine_configs_.push_back(config);
    ServerLogger::logInfo("Added default engine config: %s", config.engine_id.c_str());
}

void AutoSetupManager::set_default_download_directory(const std::string& dir) {
    download_directory_ = dir;
    ServerLogger::logInfo("Set download directory: %s", dir.c_str());
}

void AutoSetupManager::set_auto_download_enabled(bool enabled) {
    auto_download_enabled_ = enabled;
    ServerLogger::logInfo("Auto-download %s", enabled ? "enabled" : "disabled");
}

} // namespace kolosal

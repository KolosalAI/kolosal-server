#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <filesystem>

namespace kolosal {
    
    // Forward declarations
    class NodeManager;
    namespace agents {
        class YAMLConfigurableAgentManager;
        class SequentialWorkflowExecutor;
    }

    /**
     * AutoSetupManager - Handles automatic server setup and configuration
     * 
     * This class automatically:
     * - Discovers and downloads default models if missing
     * - Sets up default engines with sensible configurations
     * - Creates agent name to UUID mappings
     * - Provides workflow auto-registration with agent mapping
     * - Handles engine discovery for workflows
     */
    class AutoSetupManager {
    public:
        struct EngineConfig {
            std::string engine_id;
            std::string model_path;
            std::string download_url;
            int n_ctx = 4096;
            int n_gpu_layers = 0;
            int main_gpu_id = 0;
            int n_batch = 512;
            int n_threads = 4;
            bool auto_download = true;
        };

        struct AgentMapping {
            std::string name;
            std::string uuid;
            std::string type;
            std::string description;
        };

        AutoSetupManager(
            std::shared_ptr<NodeManager> node_manager,
            std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager
        );

        // Main auto-setup functionality
        bool perform_auto_setup();

        // Engine management
        bool auto_setup_engines();
        bool ensure_default_engine_exists();
        bool download_and_setup_default_model();
        
        // Agent management  
        bool auto_discover_agents();
        std::unordered_map<std::string, std::string> get_agent_name_to_uuid_mapping();
        
        // Workflow helpers
        bool auto_register_workflow_with_agent_mapping(const std::string& workflow_json);
        std::string map_agent_names_to_uuids_in_workflow(const std::string& workflow_json);
        
        // Configuration
        void add_default_engine_config(const EngineConfig& config);
        void set_default_download_directory(const std::string& dir);
        void set_auto_download_enabled(bool enabled);
        
        // Status queries
        bool is_default_engine_ready();
        bool are_agents_available();
        std::vector<std::string> get_available_agent_names();
        std::string get_setup_status_json();

    private:
        std::shared_ptr<NodeManager> node_manager_;
        std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager_;
        
        // Configuration
        std::vector<EngineConfig> default_engine_configs_;
        std::string download_directory_ = "downloads";
        bool auto_download_enabled_ = true;
        
        // Cache
        std::unordered_map<std::string, std::string> agent_name_to_uuid_;
        bool agents_cached_ = false;
        bool default_engine_ready_ = false;
        
        // Internal helpers
        bool download_model_if_missing(const EngineConfig& config);
        bool create_engine_from_config(const EngineConfig& config);
        void refresh_agent_cache();
        std::string get_default_model_download_url();
        std::string get_default_model_filename();
        bool validate_model_file(const std::string& path);
        void setup_default_engine_configs();
    };

} // namespace kolosal

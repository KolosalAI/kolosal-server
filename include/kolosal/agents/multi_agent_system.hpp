// File: include/kolosal/agents/multi_agent_system.hpp
#pragma once

#include "../export.hpp"
#include "yaml_config.hpp"
#include "agent_core.hpp"
#include "message_router.hpp"
#include <memory>
#include <map>
#include <atomic>
#include <mutex>

namespace kolosal::agents {

// Forward declaration
class Logger;
class ConfigurableAgentFactory;

/**
 * @brief Main multi-agent system manager
 */
class KOLOSAL_SERVER_API YAMLConfigurableAgentManager {
private:
    std::shared_ptr<Logger> logger;
    std::shared_ptr<MessageRouter> message_router;
    std::shared_ptr<ConfigurableAgentFactory> agent_factory;
    
    SystemConfig system_config;
    std::map<std::string, std::shared_ptr<AgentCore>> active_agents;
    std::atomic<bool> running{false};
    mutable std::mutex agents_mutex;  // For thread-safe agent access

    // Prevent copy/move
    YAMLConfigurableAgentManager(const YAMLConfigurableAgentManager&) = delete;
    YAMLConfigurableAgentManager& operator=(const YAMLConfigurableAgentManager&) = delete;
    YAMLConfigurableAgentManager(YAMLConfigurableAgentManager&&) = delete;
    YAMLConfigurableAgentManager& operator=(YAMLConfigurableAgentManager&&) = delete;

public:
    /**
     * @brief Construct a new YAMLConfigurableAgentManager
     */
    YAMLConfigurableAgentManager();
    ~YAMLConfigurableAgentManager();

    /**
     * @brief Load configuration from YAML file
     * @param yaml_file Path to YAML file
     * @return true on success, false on failure
     */
    [[nodiscard]] bool load_configuration(const std::string& yaml_file);

    /**
     * @brief Reload configuration from YAML file
     * @param yaml_file Path to YAML file
     * @return true on success, false on failure
     */
    [[nodiscard]] bool reload_configuration(const std::string& yaml_file);

    /**
     * @brief Start all agents and the system
     */
    void start();
    /**
     * @brief Stop all agents and the system
     */
    void stop();
    /**
     * @brief Check if the system is running
     * @return true if running
     */
    [[nodiscard]] bool is_running() const noexcept { return running.load(); }

    /**
     * @brief Create an agent from config
     * @param config Agent configuration
     * @return Agent ID string (empty if failed)
     */
    [[nodiscard]] std::string create_agent_from_config(const AgentConfig& config);
    /**
     * @brief Start an agent by ID
     * @param agent_id Agent ID
     * @return true on success
     */
    [[nodiscard]] bool start_agent(const std::string& agent_id);
    /**
     * @brief Stop an agent by ID
     * @param agent_id Agent ID
     * @return true on success
     */
    [[nodiscard]] bool stop_agent(const std::string& agent_id);
    
    /**
     * @brief Delete an agent by ID
     * @param agent_id Agent ID
     * @return true on success
     */
    [[nodiscard]] bool delete_agent(const std::string& agent_id);
    
    /**
     * @brief List all agent IDs
     * @return Vector of agent IDs
     */
    [[nodiscard]] std::vector<std::string> list_agents() const;
    /**
     * @brief Get agent by ID
     * @param agent_id Agent ID
     * @return Shared pointer to AgentCore (nullptr if not found)
     */
    [[nodiscard]] std::shared_ptr<AgentCore> get_agent(const std::string& agent_id);
    
    /**
     * @brief Get system status as a string
     * @return Status string
     */
    [[nodiscard]] std::string get_system_status() const;
    
    /**
     * @brief Demonstrate the system (for testing/demo)
     */
    void demonstrate_system();
};

/**
 * @brief Factory for creating agents from configuration
 */
class KOLOSAL_SERVER_API ConfigurableAgentFactory {
private:
    std::shared_ptr<Logger> logger;
    std::map<std::string, FunctionConfig> function_configs;

    // Prevent copy/move
    ConfigurableAgentFactory(const ConfigurableAgentFactory&) = delete;
    ConfigurableAgentFactory& operator=(const ConfigurableAgentFactory&) = delete;
    ConfigurableAgentFactory(ConfigurableAgentFactory&&) = delete;
    ConfigurableAgentFactory& operator=(ConfigurableAgentFactory&&) = delete;

public:
    /**
     * @brief Construct a new ConfigurableAgentFactory
     * @param log Logger instance
     */
    explicit ConfigurableAgentFactory(std::shared_ptr<Logger> log);

    /**
     * @brief Register a function configuration
     * @param config Function configuration
     */
    void register_function_config(const FunctionConfig& config);
    /**
     * @brief Create a function by name
     * @param function_name Name of the function
     * @return Unique pointer to AgentFunction (nullptr if not found)
     */
    [[nodiscard]] std::unique_ptr<AgentFunction> create_function(const std::string& function_name);

private:
    std::unique_ptr<AgentFunction> create_builtin_function(const FunctionConfig& config);
};

} // namespace kolosal::agents
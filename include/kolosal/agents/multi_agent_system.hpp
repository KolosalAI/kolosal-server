

// File: include/kolosal/agents/multi_agent_system.hpp
#pragma once

#include "../export.hpp"
#include "yaml_config.hpp"
#include "agent_core.hpp"
#include "message_router.hpp"
#include <memory>
#include <map>
#include <atomic>

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

public:
    YAMLConfigurableAgentManager();
    ~YAMLConfigurableAgentManager();

    // Configuration management
    bool load_configuration(const std::string& yaml_file);
    bool reload_configuration(const std::string& yaml_file);

    // Lifecycle management
    void start();
    void stop();
    bool is_running() const { return running.load(); }

    // Agent management
    std::string create_agent_from_config(const AgentConfig& config);
    bool start_agent(const std::string& agent_id);
    bool stop_agent(const std::string& agent_id);
    
    // Agent access
    std::vector<std::string> list_agents() const;
    std::shared_ptr<AgentCore> get_agent(const std::string& agent_id);
    
    // System status
    std::string get_system_status() const;
    
    // Testing and demonstration
    void demonstrate_system();
};

/**
 * @brief Factory for creating agents from configuration
 */
class KOLOSAL_SERVER_API ConfigurableAgentFactory {
private:
    std::shared_ptr<Logger> logger;
    std::map<std::string, FunctionConfig> function_configs;

public:
    ConfigurableAgentFactory(std::shared_ptr<Logger> log);

    void register_function_config(const FunctionConfig& config);
    std::unique_ptr<AgentFunction> create_function(const std::string& function_name);

private:
    std::unique_ptr<AgentFunction> create_builtin_function(const FunctionConfig& config);
};

} // namespace kolosal::agents
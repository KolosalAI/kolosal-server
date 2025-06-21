// File: include/kolosal/agents/yaml_config.hpp
#pragma once

#include "../export.hpp"
#include <string>
#include <vector>
#include <map>
#include <yaml-cpp/yaml.h>

namespace kolosal::agents {

/**
 * @brief LLM configuration parameters
 */
struct KOLOSAL_SERVER_API LLMConfig {
    std::string model_name = "gpt-4";
    std::string api_endpoint = "";
    std::string api_key = "";
    double temperature = 0.7;
    int max_tokens = 2048;
    std::vector<std::string> stop_sequences;
    std::map<std::string, std::string> custom_headers;
    int timeout_seconds = 30;
    int max_retries = 3;
    
    static LLMConfig from_yaml(const YAML::Node& node);
};

/**
 * @brief Function configuration
 */
struct KOLOSAL_SERVER_API FunctionConfig {
    std::string name;
    std::string type; // "builtin", "llm", "external_api", "custom"
    std::string description;
    std::map<std::string, std::string> parameters;
    std::string implementation; // For custom functions
    std::string endpoint; // For external APIs
    bool async_capable = true;
    int timeout_ms = 5000;
    
    static FunctionConfig from_yaml(const YAML::Node& node);
};

/**
 * @brief Agent configuration
 */
struct KOLOSAL_SERVER_API AgentConfig {
    std::string id;
    std::string name;
    std::string type;
    std::string description;
    std::string role;
    std::string system_prompt;
    std::vector<std::string> capabilities;
    std::vector<std::string> functions;
    LLMConfig llm_config;
    std::map<std::string, std::string> custom_settings;
    bool auto_start = true;
    int max_concurrent_jobs = 5;
    int heartbeat_interval_seconds = 5;
    
    static AgentConfig from_yaml(const YAML::Node& node);
};

/**
 * @brief System-wide configuration
 */
struct KOLOSAL_SERVER_API SystemConfig {
    std::vector<AgentConfig> agents;
    std::vector<FunctionConfig> functions;
    std::map<std::string, std::string> global_settings;
    int worker_threads = 4;
    int health_check_interval_seconds = 10;
    std::string log_level = "INFO";
    
    static SystemConfig from_yaml(const YAML::Node& root);
    static SystemConfig from_file(const std::string& yaml_file);
};

} // namespace kolosal::agents
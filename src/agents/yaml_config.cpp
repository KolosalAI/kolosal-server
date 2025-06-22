// File: src/agents/yaml_config.cpp
#include "kolosal/agents/yaml_config.hpp"
#include "kolosal/logger.hpp"
#include <fstream>
#include <stdexcept>

namespace kolosal::agents {

LLMConfig LLMConfig::from_yaml(const YAML::Node& node) {
    LLMConfig config;
    
    // Validate required fields
    if (!node["model_name"]) {
        throw std::runtime_error("LLM config missing required 'model_name' field");
    }
    if (!node["api_endpoint"]) {
        throw std::runtime_error("LLM config missing required 'api_endpoint' field");
    }
    
    config.model_name = node["model_name"].as<std::string>();
    config.api_endpoint = node["api_endpoint"].as<std::string>();
    
    if (node["api_key"]) {
        config.api_key = node["api_key"].as<std::string>();
    }
    
    if (node["instruction"]) {
        config.instruction = node["instruction"].as<std::string>();
    }
    
    if (node["temperature"]) {
        double temp = node["temperature"].as<double>();
        if (temp < 0.0 || temp > 1.0) {
            throw std::runtime_error("LLM temperature must be between 0.0 and 1.0");
        }
        config.temperature = temp;
    }
    
    if (node["max_tokens"]) {
        int tokens = node["max_tokens"].as<int>();
        if (tokens <= 0) {
            throw std::runtime_error("LLM max_tokens must be greater than 0");
        }
        config.max_tokens = tokens;
    }
    
    if (node["timeout_seconds"]) {
        int timeout = node["timeout_seconds"].as<int>();
        if (timeout <= 0) {
            throw std::runtime_error("LLM timeout_seconds must be greater than 0");
        }
        config.timeout_seconds = timeout;
    }
    
    if (node["max_retries"]) {
        int retries = node["max_retries"].as<int>();
        if (retries < 0) {
            throw std::runtime_error("LLM max_retries cannot be negative");
        }
        config.max_retries = retries;
    }
    
    if (node["stop_sequences"] && node["stop_sequences"].IsSequence()) {
        for (const auto& seq : node["stop_sequences"]) {
            if (!seq.IsScalar()) {
                throw std::runtime_error("Stop sequence must be a string");
            }
            config.stop_sequences.push_back(seq.as<std::string>());
        }
    }
    
    return config;
}

FunctionConfig FunctionConfig::from_yaml(const YAML::Node& node) {
    FunctionConfig config;
    
    if (!node["name"]) {
        throw std::runtime_error("Function config missing required 'name' field");
    }
    config.name = node["name"].as<std::string>();
    
    if (node["type"]) {
        config.type = node["type"].as<std::string>();
    }
    if (node["description"]) {
        config.description = node["description"].as<std::string>();
    }
    if (node["implementation"]) {
        config.implementation = node["implementation"].as<std::string>();
    }
    if (node["endpoint"]) {
        config.endpoint = node["endpoint"].as<std::string>();
    }
    if (node["async_capable"]) {
        config.async_capable = node["async_capable"].as<bool>();
    }
    if (node["timeout_ms"]) {
        config.timeout_ms = node["timeout_ms"].as<int>();
    }
    
    if (node["parameters"] && node["parameters"].IsSequence()) {
        for (const auto& param : node["parameters"]) {
            if (param["name"] && param["type"]) {
                config.parameters[param["name"].as<std::string>()] = param["type"].as<std::string>();
            }
        }
    }
    
    return config;
}

AgentConfig AgentConfig::from_yaml(const YAML::Node& node) {
    AgentConfig config;
    
    if (!node["name"]) {
        throw std::runtime_error("Agent config missing required 'name' field");
    }
    config.name = node["name"].as<std::string>();
    
    if (node["type"]) {
        config.type = node["type"].as<std::string>();
    }
    if (node["role"]) {
        config.role = node["role"].as<std::string>();
    }
    if (node["system_prompt"]) {
        config.system_prompt = node["system_prompt"].as<std::string>();
    }
    if (node["auto_start"]) {
        config.auto_start = node["auto_start"].as<bool>();
    }
    if (node["max_concurrent_jobs"]) {
        config.max_concurrent_jobs = node["max_concurrent_jobs"].as<int>();
    }
    if (node["heartbeat_interval_seconds"]) {
        config.heartbeat_interval_seconds = node["heartbeat_interval_seconds"].as<int>();
    }
    
    if (node["capabilities"] && node["capabilities"].IsSequence()) {
        for (const auto& cap : node["capabilities"]) {
            config.capabilities.push_back(cap.as<std::string>());
        }
    }
    
    if (node["functions"] && node["functions"].IsSequence()) {
        for (const auto& func : node["functions"]) {
            config.functions.push_back(func.as<std::string>());
        }
    }
      if (node["llm_config"]) {
        config.llm_config = LLMConfig::from_yaml(node["llm_config"]);
    }
    
    if (node["custom_settings"] && node["custom_settings"].IsMap()) {
        for (const auto& setting : node["custom_settings"]) {
            config.custom_settings[setting.first.as<std::string>()] = setting.second.as<std::string>();
        }
    }
    
    return config;
}

SystemConfig SystemConfig::from_yaml(const YAML::Node& root) {
    if (!root.IsMap()) {
        throw std::runtime_error("Root YAML node must be a map");
    }

    SystemConfig config;
    
    if (root["system"]) {
        const auto& system = root["system"];
        if (!system.IsMap()) {
            throw std::runtime_error("System configuration must be a map");
        }
        
        if (system["worker_threads"]) {
            int threads = system["worker_threads"].as<int>();
            if (threads <= 0) {
                throw std::runtime_error("worker_threads must be greater than 0");
            }
            config.worker_threads = threads;
        }
        
        if (system["health_check_interval_seconds"]) {
            int interval = system["health_check_interval_seconds"].as<int>();
            if (interval <= 0) {
                throw std::runtime_error("health_check_interval_seconds must be greater than 0");
            }
            config.health_check_interval_seconds = interval;
        }
        
        if (system["log_level"]) {
            std::string level = system["log_level"].as<std::string>();
            // Validate log level
            if (level != "debug" && level != "info" && level != "warn" && level != "error") {
                throw std::runtime_error("Invalid log_level. Must be one of: debug, info, warn, error");
            }
            config.log_level = level;
        }
    }
    
    if (root["agents"]) {
        if (!root["agents"].IsSequence()) {
            throw std::runtime_error("Agents configuration must be a sequence");
        }
        for (const auto& agent : root["agents"]) {
            config.agents.push_back(AgentConfig::from_yaml(agent));
        }
    }
    
    if (root["functions"]) {
        if (!root["functions"].IsSequence()) {
            throw std::runtime_error("Functions configuration must be a sequence");
        }
        for (const auto& func : root["functions"]) {
            config.functions.push_back(FunctionConfig::from_yaml(func));
        }
    }
    
    if (root["global_settings"]) {
        if (!root["global_settings"].IsMap()) {
            throw std::runtime_error("Global settings must be a map");
        }
        for (const auto& setting : root["global_settings"]) {
            if (!setting.first.IsScalar() || !setting.second.IsScalar()) {
                throw std::runtime_error("Global settings must be scalar key-value pairs");
            }
            config.global_settings[setting.first.as<std::string>()] = setting.second.as<std::string>();
        }
    }
    
    return config;
}

SystemConfig SystemConfig::from_file(const std::string& yaml_file) {
    try {
        YAML::Node root = YAML::LoadFile(yaml_file);
        return from_yaml(root);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Failed to parse YAML file '" + yaml_file + "': " + e.what());
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load YAML file '" + yaml_file + "': " + e.what());
    }
}

YAML::Node LLMConfig::to_yaml() const {
    YAML::Node node;
    
    node["model_name"] = model_name;
    node["api_endpoint"] = api_endpoint;
    
    if (!api_key.empty()) {
        node["api_key"] = api_key;
    }
    
    if (!instruction.empty()) {
        node["instruction"] = instruction;
    }
    
    node["temperature"] = temperature;
    node["max_tokens"] = max_tokens;
    node["timeout_seconds"] = timeout_seconds;
    node["max_retries"] = max_retries;
    
    if (!stop_sequences.empty()) {
        YAML::Node seq_node;
        for (const auto& seq : stop_sequences) {
            seq_node.push_back(seq);
        }
        node["stop_sequences"] = seq_node;
    }
    
    if (!custom_headers.empty()) {
        for (const auto& [key, value] : custom_headers) {
            node["custom_headers"][key] = value;
        }
    }
    
    return node;
}

YAML::Node FunctionConfig::to_yaml() const {
    YAML::Node node;
    
    node["name"] = name;
    node["type"] = type;
    
    if (!description.empty()) {
        node["description"] = description;
    }
    
    if (!parameters.empty()) {
        YAML::Node params_node;
        for (const auto& [key, value] : parameters) {
            params_node[key] = value;
        }
        node["parameters"] = params_node;
    }
    
    if (!implementation.empty()) {
        node["implementation"] = implementation;
    }
    
    if (!endpoint.empty()) {
        node["endpoint"] = endpoint;
    }
    
    node["async_capable"] = async_capable;
    node["timeout_ms"] = timeout_ms;
    
    return node;
}

YAML::Node AgentConfig::to_yaml() const {
    YAML::Node node;
    
    if (!id.empty()) {
        node["id"] = id;
    }
    
    node["name"] = name;
    
    if (!type.empty()) {
        node["type"] = type;
    }
    
    if (!description.empty()) {
        node["description"] = description;
    }
    
    if (!role.empty()) {
        node["role"] = role;
    }
    
    if (!system_prompt.empty()) {
        node["system_prompt"] = system_prompt;
    }
    
    if (!capabilities.empty()) {
        YAML::Node caps_node;
        for (const auto& cap : capabilities) {
            caps_node.push_back(cap);
        }
        node["capabilities"] = caps_node;
    }
    
    if (!functions.empty()) {
        YAML::Node funcs_node;
        for (const auto& func : functions) {
            funcs_node.push_back(func);
        }
        node["functions"] = funcs_node;
    }
    
    // Add LLM configuration
    node["llm_config"] = llm_config.to_yaml();
    
    if (!custom_settings.empty()) {
        YAML::Node settings_node;
        for (const auto& [key, value] : custom_settings) {
            settings_node[key] = value;
        }
        node["custom_settings"] = settings_node;
    }
    
    node["auto_start"] = auto_start;
    node["max_concurrent_jobs"] = max_concurrent_jobs;
    node["heartbeat_interval_seconds"] = heartbeat_interval_seconds;
    
    return node;
}

YAML::Node SystemConfig::to_yaml() const {
    YAML::Node root;
    
    // System configuration
    YAML::Node system_node;
    system_node["worker_threads"] = worker_threads;
    system_node["health_check_interval_seconds"] = health_check_interval_seconds;
    system_node["log_level"] = log_level;
    
    if (!global_settings.empty()) {
        YAML::Node global_settings_node;
        for (const auto& [key, value] : global_settings) {
            global_settings_node[key] = value;
        }
        system_node["global_settings"] = global_settings_node;
    }
    
    root["system"] = system_node;
    
    // Functions configuration
    if (!functions.empty()) {
        YAML::Node functions_node;
        for (const auto& func : functions) {
            functions_node.push_back(func.to_yaml());
        }
        root["functions"] = functions_node;
    }
    
    // Agents configuration
    if (!agents.empty()) {
        YAML::Node agents_node;
        for (const auto& agent : agents) {
            agents_node.push_back(agent.to_yaml());
        }
        root["agents"] = agents_node;
    }
    
    return root;
}

bool SystemConfig::save_to_file(const std::string& yaml_file) const {
    try {
        YAML::Node root = to_yaml();
        
        std::ofstream file(yaml_file);
        if (!file.is_open()) {
            return false;
        }
        
        file << "# Agent System Configuration for Kolosal Server\n";
        file << "# Generated configuration file\n\n";
        file << root;
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace kolosal::agents
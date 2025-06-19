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
    
    if (node["llm"]) {
        config.llm_config = LLMConfig::from_yaml(node["llm"]);
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

} // namespace kolosal::agents
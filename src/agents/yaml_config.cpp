// File: src/agents/yaml_config.cpp
#include "kolosal/agents/yaml_config.hpp"
#include "kolosal/logger.hpp"
#include <fstream>
#include <stdexcept>

namespace kolosal::agents {

LLMConfig LLMConfig::from_yaml(const YAML::Node& node) {
    LLMConfig config;
    
    if (node["model_name"]) {
        config.model_name = node["model_name"].as<std::string>();
    }
    if (node["api_endpoint"]) {
        config.api_endpoint = node["api_endpoint"].as<std::string>();
    }
    if (node["api_key"]) {
        config.api_key = node["api_key"].as<std::string>();
    }
    if (node["temperature"]) {
        config.temperature = node["temperature"].as<double>();
    }
    if (node["max_tokens"]) {
        config.max_tokens = node["max_tokens"].as<int>();
    }
    if (node["timeout_seconds"]) {
        config.timeout_seconds = node["timeout_seconds"].as<int>();
    }
    if (node["max_retries"]) {
        config.max_retries = node["max_retries"].as<int>();
    }
    
    if (node["stop_sequences"] && node["stop_sequences"].IsSequence()) {
        for (const auto& seq : node["stop_sequences"]) {
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
    SystemConfig config;
    
    if (root["system"]) {
        const auto& system = root["system"];
        if (system["worker_threads"]) {
            config.worker_threads = system["worker_threads"].as<int>();
        }
        if (system["health_check_interval_seconds"]) {
            config.health_check_interval_seconds = system["health_check_interval_seconds"].as<int>();
        }
        if (system["log_level"]) {
            config.log_level = system["log_level"].as<std::string>();
        }
    }
    
    if (root["agents"] && root["agents"].IsSequence()) {
        for (const auto& agent : root["agents"]) {
            config.agents.push_back(AgentConfig::from_yaml(agent));
        }
    }
    
    if (root["functions"] && root["functions"].IsSequence()) {
        for (const auto& func : root["functions"]) {
            config.functions.push_back(FunctionConfig::from_yaml(func));
        }
    }
    
    if (root["global_settings"] && root["global_settings"].IsMap()) {
        for (const auto& setting : root["global_settings"]) {
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
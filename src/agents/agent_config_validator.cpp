#include "kolosal/agents/agent_config_validator.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <filesystem>
#include <regex>
#include <algorithm>

namespace kolosal::agents {

AgentConfigValidator::ValidationResult AgentConfigValidator::validate_system_config(const SystemConfig& config) {
    ValidationResult result;
    
    // Validate worker threads
    if (config.worker_threads < 1) {
        result.errors.push_back("Worker threads must be at least 1");
        result.is_valid = false;
    } else if (config.worker_threads > std::thread::hardware_concurrency() * 2) {
        result.warnings.push_back("Worker threads (" + std::to_string(config.worker_threads) + 
                                 ") exceeds 2x CPU cores (" + std::to_string(std::thread::hardware_concurrency()) + ")");
    }
    
    // Validate health check interval
    if (config.health_check_interval_seconds < 5) {
        result.warnings.push_back("Health check interval is very frequent (< 5 seconds), may impact performance");
    } else if (config.health_check_interval_seconds > 300) {
        result.warnings.push_back("Health check interval is very long (> 5 minutes), issues may not be detected quickly");
    }
    
    // Validate log level
    std::vector<std::string> valid_log_levels = {"debug", "info", "warn", "error"};
    if (std::find(valid_log_levels.begin(), valid_log_levels.end(), config.log_level) == valid_log_levels.end()) {
        result.errors.push_back("Invalid log level: " + config.log_level + ". Must be one of: debug, info, warn, error");
        result.is_valid = false;
    }
    
    // Validate global settings
    auto timeout_it = config.global_settings.find("default_timeout_ms");
    if (timeout_it != config.global_settings.end()) {
        int timeout_ms = std::stoi(timeout_it->second);
        if (timeout_ms < 1000) {
            result.warnings.push_back("Default timeout is very short (< 1 second), may cause frequent timeouts");
        } else if (timeout_ms > 300000) {
            result.warnings.push_back("Default timeout is very long (> 5 minutes), may cause resource issues");
        }
    }
    
    auto retries_it = config.global_settings.find("max_retries");
    if (retries_it != config.global_settings.end()) {
        int max_retries = std::stoi(retries_it->second);
        if (max_retries < 0) {
            result.errors.push_back("Max retries cannot be negative");
            result.is_valid = false;
        } else if (max_retries > 10) {
            result.warnings.push_back("Max retries is very high (> 10), may cause long delays on failures");
        }
    }
    
    // Performance suggestions
    auto metrics_it = config.global_settings.find("enable_metrics");
    if (metrics_it != config.global_settings.end() && metrics_it->second == "true") {
        result.suggestions.push_back("Metrics are enabled - consider using monitoring endpoints for system health");
    }
    
    return result;
}

AgentConfigValidator::ValidationResult AgentConfigValidator::validate_inference_engines(const std::vector<InferenceEngineConfig>& engines) {
    ValidationResult result;
    
    if (engines.empty()) {
        result.warnings.push_back("No inference engines configured - agents with LLM functions may not work");
    }
    
    std::vector<std::string> engine_names;
    for (const auto& engine : engines) {
        // Check for duplicate names
        if (std::find(engine_names.begin(), engine_names.end(), engine.name) != engine_names.end()) {
            result.errors.push_back("Duplicate inference engine name: " + engine.name);
            result.is_valid = false;
        }
        engine_names.push_back(engine.name);
        
        // Validate engine name
        if (engine.name.empty()) {
            result.errors.push_back("Inference engine name cannot be empty");
            result.is_valid = false;
            continue;
        }
        
        // Validate model path
        if (engine.model_path.empty()) {
            result.errors.push_back("Model path cannot be empty for engine: " + engine.name);
            result.is_valid = false;
        } else if (!is_valid_url(engine.model_path) && !is_valid_path(engine.model_path)) {
            result.warnings.push_back("Model path may not be valid for engine " + engine.name + ": " + engine.model_path);
        }
        
        // Validate settings
        if (engine.context_size < 512) {
            result.warnings.push_back("Very small context size for engine " + engine.name + " (< 512 tokens)");
        } else if (engine.context_size > 32768) {
            result.warnings.push_back("Very large context size for engine " + engine.name + " (> 32K tokens) - may consume excessive memory");
        }
        
        if (engine.batch_size < 1) {
            result.errors.push_back("Batch size must be at least 1 for engine: " + engine.name);
            result.is_valid = false;
        } else if (engine.batch_size > 2048) {
            result.warnings.push_back("Very large batch size for engine " + engine.name + " - may consume excessive memory");
        }
        
        if (!is_valid_thread_count(engine.threads)) {
            result.warnings.push_back("Thread count for engine " + engine.name + " may not be optimal");
        }
        
        if (engine.gpu_layers < 0) {
            result.errors.push_back("GPU layers cannot be negative for engine: " + engine.name);
            result.is_valid = false;
        }
    }
    
    // Check for at least one auto-load engine (informational only)
    bool has_auto_load = std::any_of(engines.begin(), engines.end(), 
                                    [](const InferenceEngineConfig& e) { return e.auto_load; });
    if (!has_auto_load && !engines.empty()) {
        result.suggestions.push_back("All inference engines are set to manual loading - consider enabling auto_load for frequently used engines");
    }
    
    return result;
}

AgentConfigValidator::ValidationResult AgentConfigValidator::validate_agent_config(const AgentConfig& agent) {
    ValidationResult result;
    
    // Validate basic fields
    if (agent.name.empty()) {
        result.errors.push_back("Agent name cannot be empty");
        result.is_valid = false;
    }
    
    if (agent.type.empty()) {
        result.errors.push_back("Agent type cannot be empty for agent: " + agent.name);
        result.is_valid = false;
    }
    
    if (agent.role.empty()) {
        result.warnings.push_back("Agent role is empty for agent: " + agent.name);
    }
    
    if (agent.system_prompt.empty()) {
        result.warnings.push_back("System prompt is empty for agent: " + agent.name + " - may affect LLM performance");
    }
    
    // Validate LLM config - model_name is now optional since it can be specified at request time
    if (!is_valid_url(agent.llm_config.api_endpoint) && !agent.llm_config.api_endpoint.empty()) {
        result.warnings.push_back("Invalid API endpoint for agent " + agent.name + ": " + agent.llm_config.api_endpoint);
    }
    
    if (!is_valid_temperature(agent.llm_config.temperature)) {
        result.warnings.push_back("Temperature out of range for agent " + agent.name + " (should be 0.0-2.0)");
    }
    
    if (agent.llm_config.max_tokens < 1) {
        result.errors.push_back("Max tokens must be positive for agent: " + agent.name);
        result.is_valid = false;
    } else if (agent.llm_config.max_tokens > 8192) {
        result.warnings.push_back("Very high max tokens for agent " + agent.name + " - may consume excessive resources");
    }
    
    if (!is_valid_timeout(agent.llm_config.timeout_seconds * 1000)) {
        result.warnings.push_back("Timeout may be too short or too long for agent: " + agent.name);
    }
    
    // Validate concurrency settings
    if (agent.max_concurrent_jobs < 1) {
        result.errors.push_back("Max concurrent jobs must be at least 1 for agent: " + agent.name);
        result.is_valid = false;
    } else if (agent.max_concurrent_jobs > 20) {
        result.warnings.push_back("Very high concurrent job limit for agent " + agent.name + " - may impact performance");
    }
    
    if (agent.heartbeat_interval_seconds < 1) {
        result.errors.push_back("Heartbeat interval must be at least 1 second for agent: " + agent.name);
        result.is_valid = false;
    }
    
    // Validate capabilities and functions
    if (agent.capabilities.empty()) {
        result.warnings.push_back("No capabilities defined for agent: " + agent.name);
    }
    
    if (agent.functions.empty()) {
        result.warnings.push_back("No functions defined for agent: " + agent.name + " - agent may not be useful");
    }
    
    return result;
}

AgentConfigValidator::ValidationResult AgentConfigValidator::validate_function_configs(const std::vector<FunctionConfig>& functions) {
    ValidationResult result;
    
    std::vector<std::string> function_names;
    for (const auto& func : functions) {
        // Check for duplicate names
        if (std::find(function_names.begin(), function_names.end(), func.name) != function_names.end()) {
            result.errors.push_back("Duplicate function name: " + func.name);
            result.is_valid = false;
        }
        function_names.push_back(func.name);
        
        // Validate function fields
        if (func.name.empty()) {
            result.errors.push_back("Function name cannot be empty");
            result.is_valid = false;
            continue;
        }
        
        if (func.type.empty()) {
            result.errors.push_back("Function type cannot be empty for function: " + func.name);
            result.is_valid = false;
        }
        
        if (func.description.empty()) {
            result.warnings.push_back("Function description is empty for: " + func.name);
        }
        
        // Validate timeout
        if (!is_valid_timeout(func.timeout_ms)) {
            result.warnings.push_back("Timeout may be inappropriate for function: " + func.name);
        }
        
        // Validate external API endpoints
        if (func.type == "external_api" && !is_valid_url(func.endpoint)) {
            result.errors.push_back("Invalid endpoint URL for external API function: " + func.name);
            result.is_valid = false;
        }
    }
    
    // Check for essential functions
    std::vector<std::string> essential_functions = {"inference", "text_processing"};
    for (const auto& essential : essential_functions) {
        if (std::find(function_names.begin(), function_names.end(), essential) == function_names.end()) {
            result.suggestions.push_back("Consider adding essential function: " + essential);
        }
    }
    
    return result;
}

std::vector<AgentConfigValidator::InferenceEngineStatus> AgentConfigValidator::check_inference_engine_health() {
    std::vector<InferenceEngineStatus> statuses;
    
    // During agent system initialization, the server isn't fully ready yet
    // Attempting to check engine health at this stage can cause crashes
    // Return a simple status indicating health check will be done later
    InferenceEngineStatus status;
    status.name = "initialization_pending";
    status.available = false;
    status.healthy = false;
    status.status_message = "Engine health check skipped during initialization - will be checked after server startup";
    statuses.push_back(status);
    
    return statuses;
}

bool AgentConfigValidator::validate_agent_dependencies(const AgentConfig& agent, const std::vector<FunctionConfig>& available_functions) {
    std::vector<std::string> available_function_names;
    for (const auto& func : available_functions) {
        available_function_names.push_back(func.name);
    }
    
    for (const auto& required_function : agent.functions) {
        if (std::find(available_function_names.begin(), available_function_names.end(), required_function) == available_function_names.end()) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> AgentConfigValidator::suggest_performance_optimizations(const SystemConfig& config) {
    std::vector<std::string> suggestions;
    
    auto cpu_cores = std::thread::hardware_concurrency();
    
    if (config.worker_threads != cpu_cores) {
        suggestions.push_back("Consider setting worker_threads to " + std::to_string(cpu_cores) + 
                             " (number of CPU cores) for optimal performance");
    }
    
    if (config.health_check_interval_seconds < 30) {
        suggestions.push_back("Consider increasing health_check_interval to 30+ seconds to reduce overhead");
    }
    
    if (!config.global_settings.empty()) {
        auto metrics_it = config.global_settings.find("enable_metrics");
        if (metrics_it != config.global_settings.end() && metrics_it->second != "true") {
            suggestions.push_back("Enable metrics for better monitoring and debugging capabilities");
        }
    } else {
        suggestions.push_back("Enable metrics for better monitoring and debugging capabilities");
    }
    
    return suggestions;
}

std::vector<std::string> AgentConfigValidator::suggest_agent_improvements(const AgentConfig& agent) {
    std::vector<std::string> suggestions;
    
    if (agent.llm_config.temperature == 0.0) {
        suggestions.push_back("Consider using temperature > 0.0 for more creative responses");
    } else if (agent.llm_config.temperature > 1.0) {
        suggestions.push_back("High temperature (" + std::to_string(agent.llm_config.temperature) + 
                             ") may produce inconsistent results");
    }
    
    if (agent.max_concurrent_jobs == 1) {
        suggestions.push_back("Consider increasing max_concurrent_jobs for better throughput");
    }
    
    if (agent.functions.size() < 3) {
        suggestions.push_back("Consider adding more functions to increase agent capabilities");
    }
    
    return suggestions;
}

// Private helper methods
bool AgentConfigValidator::is_valid_path(const std::string& path) {
    if (path.empty()) return false;
    
    // Check if it's an absolute path or relative path that could exist
    try {
        std::filesystem::path p(path);
        return !path.empty() && (p.is_absolute() || path.find("..") == std::string::npos);
    } catch (...) {
        return false;
    }
}

bool AgentConfigValidator::is_valid_url(const std::string& url) {
    if (url.empty()) return false;
    
    std::regex url_pattern(R"(^https?://[^\s/$.?#].[^\s]*$)", std::regex_constants::icase);
    return std::regex_match(url, url_pattern);
}

bool AgentConfigValidator::is_valid_timeout(int timeout_ms) {
    return timeout_ms >= 1000 && timeout_ms <= 600000; // 1 second to 10 minutes
}

bool AgentConfigValidator::is_valid_thread_count(int threads) {
    auto max_threads = std::thread::hardware_concurrency();
    return threads >= 1 && threads <= static_cast<int>(max_threads * 2);
}

bool AgentConfigValidator::is_valid_temperature(double temperature) {
    return temperature >= 0.0 && temperature <= 2.0;
}

bool AgentConfigValidator::is_valid_port(int port) {
    return port >= 1024 && port <= 65535; // Avoid system ports
}

} // namespace kolosal::agents

#pragma once

#include "../export.hpp"
#include "yaml_config.hpp"
#include <string>
#include <vector>
#include <memory>

namespace kolosal::agents {

/**
 * @brief Validates agent configuration and provides diagnostics
 */
class KOLOSAL_SERVER_API AgentConfigValidator {
public:
    struct ValidationResult {
        bool is_valid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<std::string> suggestions;
    };

    struct InferenceEngineStatus {
        std::string name;
        bool available = false;
        bool healthy = false;
        std::string status_message;
        std::string model_path;
    };

    static ValidationResult validate_system_config(const SystemConfig& config);
    static ValidationResult validate_inference_engines(const std::vector<InferenceEngineConfig>& engines);
    static ValidationResult validate_agent_config(const AgentConfig& agent);
    static ValidationResult validate_function_configs(const std::vector<FunctionConfig>& functions);
    
    // Runtime validation
    static std::vector<InferenceEngineStatus> check_inference_engine_health();
    static bool validate_agent_dependencies(const AgentConfig& agent, const std::vector<FunctionConfig>& available_functions);
    
    // Configuration suggestions
    static std::vector<std::string> suggest_performance_optimizations(const SystemConfig& config);
    static std::vector<std::string> suggest_agent_improvements(const AgentConfig& agent);

private:
    static bool is_valid_path(const std::string& path);
    static bool is_valid_url(const std::string& url);
    static bool is_valid_timeout(int timeout_ms);
    static bool is_valid_thread_count(int threads);
    static bool is_valid_temperature(double temperature);
    static bool is_valid_port(int port);
};

} // namespace kolosal::agents

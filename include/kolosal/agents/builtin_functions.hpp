// File: include/kolosal/agents/builtin_functions.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include "agent_data.hpp"
#include "yaml_config.hpp"
#include <memory>

namespace kolosal::agents {

/**
 * @brief Basic arithmetic function
 */
class KOLOSAL_SERVER_API AddFunction : public AgentFunction {
public:
    std::string get_name() const override { return "add"; }
    std::string get_description() const override { return "Add two numbers"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Echo function with optional transformations
 */
class KOLOSAL_SERVER_API EchoFunction : public AgentFunction {
public:
    std::string get_name() const override { return "echo"; }
    std::string get_description() const override { return "Echo a message with optional processing"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Delay/sleep function
 */
class KOLOSAL_SERVER_API DelayFunction : public AgentFunction {
public:
    std::string get_name() const override { return "delay"; }
    std::string get_description() const override { return "Wait for specified milliseconds"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Text analysis function (supports both text_analysis and text_processing names)
 */
class KOLOSAL_SERVER_API TextAnalysisFunction : public AgentFunction {
public:
    std::string get_name() const override { return "text_analysis"; }
    std::string get_description() const override { return "Analyze text for word count, character count, and sentiment"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Text processing function (alias for TextAnalysisFunction to match YAML config)
 */
class KOLOSAL_SERVER_API TextProcessingFunction : public AgentFunction {
public:
    std::string get_name() const override { return "text_processing"; }
    std::string get_description() const override { return "Process and analyze text content"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Data transformation function
 */
class KOLOSAL_SERVER_API DataTransformFunction : public AgentFunction {
public:
    std::string get_name() const override { return "data_transform"; }
    std::string get_description() const override { return "Transform data arrays with various operations"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Data analysis function
 */
class KOLOSAL_SERVER_API DataAnalysisFunction : public AgentFunction {
public:
    std::string get_name() const override { return "data_analysis"; }
    std::string get_description() const override { return "Analyze structured data and extract insights"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Inference function that bridges to Kolosal inference engines
 */
class KOLOSAL_SERVER_API InferenceFunction : public AgentFunction {
private:
    std::string engine_id;
    
public:
    InferenceFunction(const std::string& engine = "default");
    
    std::string get_name() const override { return "inference"; }
    std::string get_description() const override { return "Run inference using the specified engine"; }
    std::string get_type() const override { return "inference"; }
    FunctionResult execute(const AgentData& params) override;
    
    void set_engine_id(const std::string& engine) { engine_id = engine; }
    const std::string& get_engine_id() const { return engine_id; }
};

/**
 * @brief LLM-based function that uses language model for execution
 */
class KOLOSAL_SERVER_API LLMFunction : public AgentFunction {
private:
    std::string name;
    std::string description;
    std::string system_prompt;
    LLMConfig llm_config;

public:
    LLMFunction(const std::string& func_name, const std::string& func_desc, 
               const std::string& prompt, const LLMConfig& config);

    std::string get_name() const override { return name; }
    std::string get_description() const override { return description; }
    std::string get_type() const override { return "llm"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief External API function
 */
class KOLOSAL_SERVER_API ExternalAPIFunction : public AgentFunction {
private:
    std::string name;
    std::string description;
    std::string endpoint;
    std::map<std::string, std::string> headers;

public:
    ExternalAPIFunction(const std::string& func_name, const std::string& func_desc, 
                       const std::string& api_endpoint);

    std::string get_name() const override { return name; }
    std::string get_description() const override { return description; }
    std::string get_type() const override { return "external_api"; }
    FunctionResult execute(const AgentData& params) override;
};

} // namespace kolosal::agents

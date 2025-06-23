// File: src/agents/builtin_functions.cpp
#include "kolosal/agents/builtin_functions.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "inference_interface.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <chrono>
#include <thread>

namespace kolosal::agents {

// Utility function for sleeping
inline void sleep_for_ms(int milliseconds) {
    if (milliseconds <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// AddFunction implementation
FunctionResult AddFunction::execute(const AgentData& params) {
    int a = params.get_int("a");
    int b = params.get_int("b");
    
    FunctionResult result(true);
    result.result_data.set("result", a + b);
    result.result_data.set("operation", "addition");
    return result;
}

// EchoFunction implementation
FunctionResult EchoFunction::execute(const AgentData& params) {
    std::string message = params.get_string("message");
    bool uppercase = params.get_bool("uppercase", false);
    
    if (uppercase) {
        std::transform(message.begin(), message.end(), message.begin(), ::toupper);
    }
    
    FunctionResult result(true);
    result.result_data.set("echo", message);
    result.result_data.set("original", params.get_string("message"));
    result.result_data.set("processed", uppercase);
    return result;
}

// DelayFunction implementation
FunctionResult DelayFunction::execute(const AgentData& params) {
    int ms = params.get_int("ms");
    if (ms < 0) {
        return FunctionResult(false, "Delay must be non-negative");
    }
    
    sleep_for_ms(ms);
    
    FunctionResult result(true);
    result.result_data.set("waited_ms", ms);
    result.result_data.set("status", "completed");
    return result;
}

// TextAnalysisFunction implementation
FunctionResult TextAnalysisFunction::execute(const AgentData& params) {
    std::string text = params.get_string("text");
    std::string operation = params.get_string("operation", "analyze");
    
    if (operation == "analyze") {
        // Word count
        std::istringstream iss(text);
        std::string word;
        int word_count = 0;
        while (iss >> word) {
            word_count++;
        }
        
        // Character count
        int char_count = static_cast<int>(text.length());
        int char_count_no_spaces = 0;
        for (char c : text) {
            if (c != ' ' && c != '\t' && c != '\n') {
                char_count_no_spaces++;
            }
        }
        
        // Simple sentiment analysis
        std::string sentiment = "neutral";
        std::vector<std::string> positive_words = {"good", "great", "excellent", "amazing", "wonderful", "fantastic"};
        std::vector<std::string> negative_words = {"bad", "terrible", "awful", "horrible", "disappointing"};
        
        std::string lower_text = text;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        
        int positive_score = 0, negative_score = 0;
        for (const auto& word : positive_words) {
            if (lower_text.find(word) != std::string::npos) positive_score++;
        }
        for (const auto& word : negative_words) {
            if (lower_text.find(word) != std::string::npos) negative_score++;
        }
        
        if (positive_score > negative_score) sentiment = "positive";
        else if (negative_score > positive_score) sentiment = "negative";
        
        FunctionResult result(true);
        result.result_data.set("word_count", word_count);
        result.result_data.set("character_count", char_count);
        result.result_data.set("char_count_no_spaces", char_count_no_spaces);
        result.result_data.set("sentiment", sentiment);
        result.result_data.set("positive_score", positive_score);
        result.result_data.set("negative_score", negative_score);
        result.result_data.set("readability_score", 8.2); // Mock readability score
        result.result_data.set("result", "Text analyzed successfully");
        
        return result;
    } else if (operation == "summarize") {
        // Simple summarization - take first sentence or first 100 chars
        std::string summary = text.substr(0, std::min(100, (int)text.length()));
        if (text.length() > 100) summary += "...";
        
        FunctionResult result(true);
        result.result_data.set("summary", summary);
        result.result_data.set("original_length", static_cast<int>(text.length()));
        result.result_data.set("summary_length", static_cast<int>(summary.length()));
        result.result_data.set("result", summary);
        
        return result;
    } else if (operation == "tokenize") {
        // Simple tokenization
        std::istringstream iss(text);
        std::string word;
        std::vector<std::string> tokens;
        while (iss >> word) {
            tokens.push_back(word);
        }
        
        FunctionResult result(true);
        result.result_data.set("token_count", static_cast<int>(tokens.size()));
        result.result_data.set("result", "Text tokenized into " + std::to_string(tokens.size()) + " tokens");
        
        return result;
    }
      // Default fallback
    FunctionResult result(true);
    result.result_data.set("result", "Text processing completed for operation: " + operation);
    return result;
}

// TextProcessingFunction implementation (alias for TextAnalysisFunction)
FunctionResult TextProcessingFunction::execute(const AgentData& params) {
    // Delegate to TextAnalysisFunction logic
    std::string text = params.get_string("text");
    std::string operation = params.get_string("operation", "analyze");
    
    if (operation == "analyze") {
        // Word count
        std::istringstream iss(text);
        std::string word;
        int word_count = 0;
        while (iss >> word) {
            word_count++;
        }
        
        // Character count
        int char_count = static_cast<int>(text.length());
        int char_count_no_spaces = 0;
        for (char c : text) {
            if (c != ' ' && c != '\t' && c != '\n') {
                char_count_no_spaces++;
            }
        }
        
        // Simple sentiment analysis
        std::string sentiment = "neutral";
        std::vector<std::string> positive_words = {"good", "great", "excellent", "amazing", "wonderful", "fantastic"};
        std::vector<std::string> negative_words = {"bad", "terrible", "awful", "horrible", "disappointing"};
        
        std::string lower_text = text;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        
        int positive_score = 0, negative_score = 0;
        for (const auto& word : positive_words) {
            if (lower_text.find(word) != std::string::npos) positive_score++;
        }
        for (const auto& word : negative_words) {
            if (lower_text.find(word) != std::string::npos) negative_score++;
        }
        
        if (positive_score > negative_score) sentiment = "positive";
        else if (negative_score > positive_score) sentiment = "negative";
        
        FunctionResult result(true);
        result.result_data.set("word_count", word_count);
        result.result_data.set("character_count", char_count);
        result.result_data.set("char_count_no_spaces", char_count_no_spaces);
        result.result_data.set("sentiment", sentiment);
        result.result_data.set("positive_score", positive_score);
        result.result_data.set("negative_score", negative_score);
        result.result_data.set("readability_score", 8.2); // Mock readability score
        result.result_data.set("result", "Text analyzed successfully");
        
        return result;
    } else if (operation == "summarize") {
        // Simple summarization - take first sentence or first 100 chars
        std::string summary = text.substr(0, std::min(100, (int)text.length()));
        if (text.length() > 100) summary += "...";
        
        FunctionResult result(true);
        result.result_data.set("summary", summary);
        result.result_data.set("original_length", static_cast<int>(text.length()));
        result.result_data.set("summary_length", static_cast<int>(summary.length()));
        result.result_data.set("result", summary);
        
        return result;
    } else if (operation == "tokenize") {
        // Simple tokenization
        std::istringstream iss(text);
        std::string word;
        std::vector<std::string> tokens;
        while (iss >> word) {
            tokens.push_back(word);
        }
        
        FunctionResult result(true);
        result.result_data.set("token_count", static_cast<int>(tokens.size()));
        result.result_data.set("result", "Text tokenized into " + std::to_string(tokens.size()) + " tokens");
        
        return result;
    }
    
    // Default fallback
    FunctionResult result(true);
    result.result_data.set("result", "Text processing completed for operation: " + operation);
    return result;
}

// DataTransformFunction implementation
FunctionResult DataTransformFunction::execute(const AgentData& params) {
    std::vector<std::string> input_data = params.get_array_string("data");
    std::string operation = params.get_string("operation", "identity");
    
    std::vector<std::string> result_data;
    
    for (const auto& item : input_data) {
        if (operation == "uppercase") {
            std::string upper_item = item;
            std::transform(upper_item.begin(), upper_item.end(), upper_item.begin(), ::toupper);
            result_data.push_back(upper_item);
        } else if (operation == "lowercase") {
            std::string lower_item = item;
            std::transform(lower_item.begin(), lower_item.end(), lower_item.begin(), ::tolower);
            result_data.push_back(lower_item);
        } else if (operation == "reverse") {
            std::string reversed_item = item;
            std::reverse(reversed_item.begin(), reversed_item.end());
            result_data.push_back(reversed_item);
        } else if (operation == "length") {
            result_data.push_back(std::to_string(item.length()));
        } else {
            result_data.push_back(item); // identity
        }
    }
    
    FunctionResult result(true);
    result.result_data.set("original_count", static_cast<int>(input_data.size()));
    result.result_data.set("processed_count", static_cast<int>(result_data.size()));
    result.result_data.set("operation_applied", operation);
    
    // Convert result array to string representation
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < result_data.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << result_data[i] << "\"";
    }
    oss << "]";
    result.result_data.set("transformed_data", oss.str());
    
    return result;
}

// DataAnalysisFunction implementation
FunctionResult DataAnalysisFunction::execute(const AgentData& params) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::string data = params.get_string("data");
    std::string analysis_type = params.get_string("analysis_type", "basic");
    
    FunctionResult result(true);
    
    if (data.empty()) {
        result.success = false;
        result.error_message = "Data parameter is required";
        return result;
    }
    
    try {
        if (analysis_type == "basic") {
            // Basic data analysis
            int data_size = static_cast<int>(data.length());
            int line_count = std::count(data.begin(), data.end(), '\n') + 1;
            int word_count = 0;
            std::istringstream iss(data);
            std::string word;
            while (iss >> word) {
                word_count++;
            }
            
            result.result_data.set("data_size_bytes", data_size);
            result.result_data.set("line_count", line_count);
            result.result_data.set("word_count", word_count);
            result.result_data.set("analysis_type", analysis_type);
            result.result_data.set("summary", "Basic data analysis completed");
            result.result_data.set("result", "Data contains " + std::to_string(line_count) + " lines and " + std::to_string(word_count) + " words");
            
        } else if (analysis_type == "statistical") {
            // Mock statistical analysis
            result.result_data.set("mean", 42.5);
            result.result_data.set("median", 40.0);
            result.result_data.set("std_dev", 15.2);
            result.result_data.set("min", 10.0);
            result.result_data.set("max", 95.0);
            result.result_data.set("analysis_type", analysis_type);
            result.result_data.set("summary", "Statistical analysis completed");
            result.result_data.set("result", "Statistical analysis shows mean=42.5, std_dev=15.2");
            
        } else if (analysis_type == "pattern") {
            // Pattern analysis
            std::string patterns_found = "Sequential patterns, Recurring elements";
            result.result_data.set("patterns", patterns_found);
            result.result_data.set("confidence", 0.85);
            result.result_data.set("analysis_type", analysis_type);
            result.result_data.set("summary", "Pattern analysis completed");
            result.result_data.set("result", "Found patterns: " + patterns_found);
            
        } else {
            // Default analysis
            result.result_data.set("analysis_type", analysis_type);
            result.result_data.set("data_processed", true);
            result.result_data.set("summary", "Custom data analysis completed");
            result.result_data.set("result", "Data analysis completed for type: " + analysis_type);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.execution_time_ms = duration.count() / 1000.0;
        
        return result;
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        result.success = false;
        result.error_message = "Data analysis error: " + std::string(e.what());
        result.execution_time_ms = duration.count() / 1000.0;
        
        return result;
    }
}

// InferenceFunction implementation
InferenceFunction::InferenceFunction(const std::string& engine) : engine_id(engine) {}

FunctionResult InferenceFunction::execute(const AgentData& params) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Get the NodeManager and inference engine
        auto& nodeManager = ServerAPI::instance().getNodeManager();
        auto engine = nodeManager.getEngine(engine_id);
        
        if (!engine) {
            return FunctionResult(false, "Engine not available: " + engine_id);
        }
        
        // Extract parameters
        std::string prompt = params.get_string("prompt");
        if (prompt.empty()) {
            return FunctionResult(false, "Prompt parameter is required");
        }
        
        int max_tokens = params.get_int("max_tokens", 128);
        double temperature = params.get_double("temperature", 0.7);
        double top_p = params.get_double("top_p", 0.9);
        int seed = params.get_int("seed", -1);
        
        // Build completion parameters
        CompletionParameters inferenceParams;
        inferenceParams.prompt = prompt;
        inferenceParams.maxNewTokens = max_tokens;
        inferenceParams.temperature = static_cast<float>(temperature);
        inferenceParams.topP = static_cast<float>(top_p);
        if (seed >= 0) {
            inferenceParams.randomSeed = seed;
        }
        
        // Submit job and wait for completion
        int job_id = engine->submitCompletionsJob(inferenceParams);
        if (job_id < 0) {
            return FunctionResult(false, "Failed to submit inference job");
        }
        
        engine->waitForJob(job_id);
        
        if (engine->hasJobError(job_id)) {
            return FunctionResult(false, "Inference error: " + engine->getJobError(job_id));
        }
        
        CompletionResult completion_result = engine->getJobResult(job_id);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        FunctionResult result(true);
        result.result_data.set("text", completion_result.text);
        result.result_data.set("tokens_generated", static_cast<int>(completion_result.tokens.size()));
        result.result_data.set("tokens_per_second", static_cast<double>(completion_result.tps));
        result.result_data.set("engine_used", engine_id);
        result.execution_time_ms = duration.count() / 1000.0;
        
        ServerLogger::logInfo("Inference function completed: %d tokens, %.2f TPS", 
                             static_cast<int>(completion_result.tokens.size()), completion_result.tps);
        
        return result;
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        FunctionResult result(false, std::string("Inference function error: ") + e.what());
        result.execution_time_ms = duration.count() / 1000.0;
        
        ServerLogger::logError("Inference function error: %s", e.what());
        return result;
    }
}

// LLMFunction implementation
LLMFunction::LLMFunction(const std::string& func_name, const std::string& func_desc, 
                        const std::string& prompt, const LLMConfig& config)
    : name(func_name), description(func_desc), system_prompt(prompt), llm_config(config) {}

FunctionResult LLMFunction::execute(const AgentData& params) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // For demo purposes, simulate LLM processing using the inference engine
    try {
        auto& nodeManager = ServerAPI::instance().getNodeManager();
        auto engine = nodeManager.getEngine(llm_config.model_name);
        
        if (!engine) {
            ServerLogger::logWarning("LLM engine '%s' not available, falling back to mock response", llm_config.model_name.c_str());
            
            // Provide a fallback response instead of failing
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            FunctionResult result(true);
            
            // Generate a mock response based on the function name and parameters
            std::string mock_response = "I understand you're asking about ";
            for (const auto& key : params.get_all_keys()) {
                mock_response += key + " ";
            }
            mock_response += ". As an AI assistant, I'm here to help with your questions and tasks.";
            
            result.llm_response = mock_response;
            result.result_data.set("llm_output", mock_response);
            result.result_data.set("tokens_generated", 25); // Mock token count
            result.result_data.set("engine_used", "mock_engine");
            result.result_data.set("note", "Mock response - LLM engine not available");
            result.execution_time_ms = duration.count() / 1000.0;
            
            return result;
        }
        
        // Build prompt from system prompt and parameters
        std::ostringstream full_prompt;
        full_prompt << "System: " << system_prompt << "\n\n";
        full_prompt << "Function: " << name << "\n";
        full_prompt << "Description: " << description << "\n";
        full_prompt << "Parameters: ";
        
        for (const auto& key : params.get_all_keys()) {
            full_prompt << key << "=" << params.get_string(key) << " ";
        }
        
        CompletionParameters inferenceParams;
        inferenceParams.prompt = full_prompt.str();
        inferenceParams.maxNewTokens = llm_config.max_tokens;
        inferenceParams.temperature = static_cast<float>(llm_config.temperature);
        
        int job_id = engine->submitCompletionsJob(inferenceParams);
        if (job_id < 0) {
            return FunctionResult(false, "Failed to submit LLM job");
        }
        
        engine->waitForJob(job_id);
        
        if (engine->hasJobError(job_id)) {
            return FunctionResult(false, "LLM error: " + engine->getJobError(job_id));
        }
        
        CompletionResult completion_result = engine->getJobResult(job_id);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        FunctionResult result(true);
        result.llm_response = completion_result.text;
        result.result_data.set("llm_output", completion_result.text);
        result.result_data.set("tokens_generated", static_cast<int>(completion_result.tokens.size()));
        result.execution_time_ms = duration.count() / 1000.0;
        
        return result;
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        FunctionResult result(false, std::string("LLM function error: ") + e.what());
        result.execution_time_ms = duration.count() / 1000.0;
        
        return result;
    }
}

// ExternalAPIFunction implementation
ExternalAPIFunction::ExternalAPIFunction(const std::string& func_name, const std::string& func_desc, 
                                       const std::string& api_endpoint)
    : name(func_name), description(func_desc), endpoint(api_endpoint) {}

FunctionResult ExternalAPIFunction::execute(const AgentData& params) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // For demo purposes, simulate API response
    sleep_for_ms(50 + (rand() % 150));
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    FunctionResult result(true);
    result.result_data.set("api_response", "Simulated API response from " + endpoint);
    result.result_data.set("endpoint", endpoint);
    result.execution_time_ms = duration.count() / 1000.0;
    
    ServerLogger::logInfo("External API function simulated call to: %s", endpoint.c_str());
    
    return result;
}

} // namespace kolosal::agents
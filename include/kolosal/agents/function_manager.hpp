
// File: include/kolosal/agents/function_manager.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace kolosal::agents {

// Forward declaration
class Logger;

/**
 * @brief Manages agent functions and their execution
 */
class KOLOSAL_SERVER_API FunctionManager {
private:
    std::unordered_map<std::string, std::unique_ptr<AgentFunction>> functions;
    std::shared_ptr<Logger> logger;
    mutable std::mutex functions_mutex;

public:
    FunctionManager(std::shared_ptr<Logger> log);

    bool register_function(std::unique_ptr<AgentFunction> function);
    FunctionResult execute_function(const std::string& name, const AgentData& params);
    std::vector<std::string> get_function_names() const;
    bool has_function(const std::string& name) const;
    std::string get_function_description(const std::string& name) const;
    
    // Enhanced tool management
    std::string get_available_tools_summary() const;
    std::vector<std::pair<std::string, std::string>> get_all_functions_with_descriptions() const;
};

} // namespace kolosal::agents
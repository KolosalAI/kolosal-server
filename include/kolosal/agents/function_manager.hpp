
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
};

} // namespace kolosal::agents
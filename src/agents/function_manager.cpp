// File: src/agents/function_manager.cpp
#include "kolosal/agents/function_manager.hpp"
#include "kolosal/logger.hpp"
#include <chrono>

namespace kolosal::agents {

// Bridge Logger interface to ServerLogger
class Logger {
public:
    void info(const std::string& message) { ServerLogger::logInfo("%s", message.c_str()); }
    void debug(const std::string& message) { ServerLogger::logDebug("%s", message.c_str()); }
    void warn(const std::string& message) { ServerLogger::logWarning("%s", message.c_str()); }
    void error(const std::string& message) { ServerLogger::logError("%s", message.c_str()); }
};

FunctionManager::FunctionManager(std::shared_ptr<Logger> log) : logger(log) {}

bool FunctionManager::register_function(std::unique_ptr<AgentFunction> function) {
    std::lock_guard<std::mutex> lock(functions_mutex);
    std::string name = function->get_name();
    functions[name] = std::move(function);
    logger->info("Registered function: " + name);
    return true;
}

FunctionResult FunctionManager::execute_function(const std::string& name, const AgentData& params) {
    std::lock_guard<std::mutex> lock(functions_mutex);
    auto it = functions.find(name);
    if (it == functions.end()) {
        return FunctionResult(false, "Function not found: " + name);
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    FunctionResult result = it->second->execute(params);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    if (result.execution_time_ms == 0.0) {
        result.execution_time_ms = duration.count() / 1000.0;
    }

    logger->debug("Function '" + name + "' executed in " + std::to_string(result.execution_time_ms) + "ms");
    return result;
}

std::vector<std::string> FunctionManager::get_function_names() const {
    std::lock_guard<std::mutex> lock(functions_mutex);
    std::vector<std::string> names;
    for (const auto& pair : functions) {
        names.push_back(pair.first);
    }
    return names;
}

bool FunctionManager::has_function(const std::string& name) const {
    std::lock_guard<std::mutex> lock(functions_mutex);
    return functions.find(name) != functions.end();
}

std::string FunctionManager::get_function_description(const std::string& name) const {
    std::lock_guard<std::mutex> lock(functions_mutex);
    auto it = functions.find(name);
    if (it != functions.end()) {
        return it->second->get_description();
    }
    return "";
}

} // namespace kolosal::agents
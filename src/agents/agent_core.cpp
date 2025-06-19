// File: src/agents/agent_core.cpp
#include "kolosal/agents/agent_core.hpp"
#include "kolosal/agents/builtin_functions.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/agents/server_logger_adapter.hpp"
#include <mutex>

namespace kolosal::agents {

namespace {
    std::mutex g_capabilities_mutex;  // Global mutex for thread-safe capabilities access
    std::mutex g_message_mutex;       // Global mutex for thread-safe message handling
}

AgentCore::AgentCore(const std::string& name, const std::string& type) 
    : agent_id(UUIDGenerator::generate()), 
      agent_name(name.empty() ? "Agent-" + agent_id.substr(0, 8) : name), 
      agent_type(type) {
    
    // Create logger bridge
    logger = std::make_shared<ServerLoggerAdapter>();
    
    // Initialize components
    function_manager = std::make_shared<FunctionManager>(logger);
    job_manager = std::make_shared<JobManager>(function_manager, logger);
    event_system = std::make_shared<EventSystem>(logger);
    
    // Register default functions
    function_manager->register_function(std::make_unique<AddFunction>());
    function_manager->register_function(std::make_unique<EchoFunction>());
    function_manager->register_function(std::make_unique<DelayFunction>());
    function_manager->register_function(std::make_unique<TextAnalysisFunction>());
    function_manager->register_function(std::make_unique<DataTransformFunction>());
    function_manager->register_function(std::make_unique<InferenceFunction>());
    
    logger->info("Agent created: " + agent_name + " (ID: " + agent_id.substr(0, 8) + "...)");
}

AgentCore::~AgentCore() = default;

void AgentCore::start() {
    running.store(true);
    if (logger) logger->info("Agent started: " + agent_name);
}

void AgentCore::stop() {
    running.store(false);
    if (logger) logger->info("Agent stopped: " + agent_name);
}

void AgentCore::set_message_router(std::shared_ptr<MessageRouter> router) {
    message_router = router;
    if (logger) logger->debug("Message router set for agent: " + agent_name);
}

FunctionResult AgentCore::execute_function(const std::string& name, const AgentData& params) {
    if (!function_manager) {
        FunctionResult result;
        result.success = false;
        result.error_message = "Function manager not initialized";
        result.execution_time_ms = 0.0;
        // result.result_data is default
        return result;
    }
    return function_manager->execute_function(name, params);
}

void AgentCore::send_message(const std::string& to_agent, const std::string& message_type, const AgentData& payload) {
    if (message_router) {
        AgentMessage msg(agent_id, to_agent, message_type);
        msg.id = UUIDGenerator::generate();
        msg.payload = payload;
        msg.timestamp = std::chrono::system_clock::now();
        message_router->route_message(msg);
    } else if (logger) {
        logger->warn("No message router set for agent: " + agent_name);
    }
}

void AgentCore::add_capability(const std::string& capability) {
    if (capability.empty()) {
        logger->warn("Attempted to add empty capability to " + agent_name);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_capabilities_mutex);
        // Check for duplicate capability
        if (std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end()) {
            logger->debug("Capability already exists in " + agent_name + ": " + capability);
            return;
        }
        capabilities.push_back(capability);
    }
    
    logger->debug("Capability added to " + agent_name + ": " + capability);
}

void AgentCore::handle_message(const AgentMessage& message) {
    if (!running.load()) {
        logger->warn("Ignoring message - agent is not running: " + agent_name);
        return;
    }

    std::lock_guard<std::mutex> lock(g_message_mutex);

    if (message.from_agent.empty()) {
        logger->warn("Received message with empty sender ID");
        return;
    }

    logger->debug("Received message: " + message.type + " from " + message.from_agent);
    
    try {
        // Handle standard message types
        if (message.type == "ping") {
            AgentData pong_data;
            pong_data.set("timestamp", std::to_string(std::time(nullptr)));
            send_message(message.from_agent, "pong", pong_data);
        } else if (message.type == "greeting") {
            auto greeting_msg = message.payload.get_string("message");
            if (!greeting_msg.empty()) {
                logger->info("Greeting received: " + greeting_msg);
            }
        } else if (message.type == "function_request") {
            // Handle function request inline since it's private
            std::string function_name = message.payload.get_string("function");
            if (function_name.empty()) {
                throw std::runtime_error("Missing function name in function request");
            }

            FunctionResult result = execute_function(function_name, message.payload);
            
            AgentData response_data;
            response_data.set("success", result.success);
            response_data.set("error_message", result.error_message);
            response_data.set("execution_time_ms", result.execution_time_ms);
            response_data.set("result_data", result.result_data);
            
            send_message(message.from_agent, "function_response", response_data);
        }
        
        // Emit message received event
        AgentData event_data;
        event_data.set("agent_id", agent_id);
        event_data.set("from_agent", message.from_agent);
        event_data.set("message_type", message.type);
        event_system->emit("message_received", agent_id, event_data);
    } catch (const std::exception& e) {
        logger->error("Error handling message: " + std::string(e.what()));
        
        // Send error response if appropriate
        if (message.type == "function_request") {
            AgentData error_response;
            error_response.set("success", false);
            error_response.set("error_message", "Internal error: " + std::string(e.what()));
            send_message(message.from_agent, "function_response", error_response);
        }
    }
}

std::string AgentCore::execute_function_async(const std::string& name, const AgentData& params, int priority) {
    if (!running.load()) {
        throw std::runtime_error("Cannot execute function - agent is not running");
    }

    if (name.empty()) {
        throw std::runtime_error("Function name cannot be empty");
    }

    if (priority < 0) {
        throw std::runtime_error("Priority cannot be negative");
    }

    logger->debug("Submitting async function '" + name + "' on agent " + agent_name);

    try {
        return job_manager->submit_job(name, params, priority, agent_id);
    } catch (const std::exception& e) {
        logger->error("Failed to submit async function '" + name + "': " + e.what());
        throw;
    }
}
} // namespace kolosal::agents
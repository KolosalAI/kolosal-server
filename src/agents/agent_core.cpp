// File: src/agents/agent_core.cpp
#include "kolosal/agents/agent_core.hpp"
#include "kolosal/agents/builtin_functions.hpp"
#include "kolosal/agents/logger.hpp"

namespace kolosal::agents {

AgentCore::AgentCore(const std::string& name, const std::string& type) 
    : agent_id(UUIDGenerator::generate()), 
      agent_name(name.empty() ? "Agent-" + agent_id.substr(0, 8) : name), 
      agent_type(type) {
    
    // Create logger bridge
    logger = std::make_shared<Logger>();
    
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

AgentCore::~AgentCore() {
    stop();
}

void AgentCore::set_message_router(std::shared_ptr<MessageRouter> router) {
    message_router = router;
    if (message_router) {
        message_router->register_agent_handler(agent_id, 
            [this](const AgentMessage& msg) { handle_message(msg); });
    }
}

void AgentCore::start() {
    if (running.load()) return;
    running.store(true);
    event_system->start();
    job_manager->start();
    logger->info("Agent started successfully: " + agent_name);
    
    // Emit agent started event
    AgentData event_data;
    event_data.set("agent_id", agent_id);
    event_data.set("agent_name", agent_name);
    event_system->emit("agent_started", agent_id, event_data);
}

void AgentCore::stop() {
    if (!running.load()) return;
    running.store(false);
    
    job_manager->stop();
    event_system->stop();
    
    if (message_router) {
        message_router->unregister_agent_handler(agent_id);
    }
    
    logger->info("Agent stopped: " + agent_name);
    
    // Emit agent stopped event
    AgentData event_data;
    event_data.set("agent_id", agent_id);
    event_data.set("agent_name", agent_name);
    event_system->emit("agent_stopped", agent_id, event_data);
}

void AgentCore::add_capability(const std::string& capability) {
    capabilities.push_back(capability);
    logger->debug("Capability added to " + agent_name + ": " + capability);
}

FunctionResult AgentCore::execute_function(const std::string& name, const AgentData& params) {
    logger->debug("Executing function '" + name + "' on agent " + agent_name);
    
    FunctionResult result = function_manager->execute_function(name, params);
    
    // Emit function execution event
    AgentData event_data;
    event_data.set("agent_id", agent_id);
    event_data.set("function_name", name);
    event_data.set("success", result.success);
    event_data.set("execution_time_ms", result.execution_time_ms);
    event_system->emit("function_executed", agent_id, event_data);
    
    return result;
}

std::string AgentCore::execute_function_async(const std::string& name, const AgentData& params, int priority) {
    logger->debug("Submitting async function '" + name + "' on agent " + agent_name);
    return job_manager->submit_job(name, params, priority, agent_id);
}

void AgentCore::send_message(const std::string& to_agent, const std::string& message_type, 
                            const AgentData& payload) {
    if (message_router) {
        AgentMessage msg(agent_id, to_agent, message_type);
        msg.payload = payload;
        message_router->route_message(msg);
        
        logger->debug("Message sent from " + agent_name + " to " + to_agent + " (type: " + message_type + ")");
    } else {
        logger->warn("Cannot send message - no message router configured");
    }
}

void AgentCore::broadcast_message(const std::string& message_type, const AgentData& payload) {
    if (message_router) {
        AgentMessage msg(agent_id, "", message_type);
        msg.payload = payload;
        message_router->broadcast_message(msg);
        
        logger->debug("Broadcast message sent from " + agent_name + " (type: " + message_type + ")");
    } else {
        logger->warn("Cannot broadcast message - no message router configured");
    }
}

void AgentCore::handle_message(const AgentMessage& message) {
    logger->debug("Received message: " + message.type + " from " + message.from_agent);
    
    // Handle standard message types
    if (message.type == "ping") {
        AgentData pong_data;
        pong_data.set("timestamp", std::to_string(std::time(nullptr)));
        send_message(message.from_agent, "pong", pong_data);
    } else if (message.type == "greeting") {
        logger->info("Greeting received: " + message.payload.get_string("message"));
    } else if (message.type == "function_request") {
        // Handle function execution requests
        std::string function_name = message.payload.get_string("function");
        if (!function_name.empty()) {
            FunctionResult result = execute_function(function_name, message.payload);
            
            AgentData response_data;
            response_data.set("success", result.success);
            response_data.set("error_message", result.error_message);
            response_data.set("execution_time_ms", result.execution_time_ms);
            
            send_message(message.from_agent, "function_response", response_data);
        }
    }
    
    // Emit message received event
    AgentData event_data;
    event_data.set("agent_id", agent_id);
    event_data.set("from_agent", message.from_agent);
    event_data.set("message_type", message.type);
    event_system->emit("message_received", agent_id, event_data);
}

} // namespace kolosal::agents
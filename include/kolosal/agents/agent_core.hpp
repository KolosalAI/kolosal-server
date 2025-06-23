

// File: include/kolosal/agents/agent_core.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include "function_manager.hpp"
#include "job_manager.hpp"
#include "event_system.hpp"
#include "message_router.hpp"
#include <memory>
#include <atomic>
#include <vector>
#include <mutex>

namespace kolosal::agents {

// Forward declaration
class Logger;

/**
 * @brief Core agent implementation
 */
class KOLOSAL_SERVER_API AgentCore : public std::enable_shared_from_this<AgentCore> {
private:
    std::shared_ptr<Logger> logger;
    std::shared_ptr<FunctionManager> function_manager;
    std::shared_ptr<JobManager> job_manager;
    std::shared_ptr<EventSystem> event_system;
    std::shared_ptr<MessageRouter> message_router;    std::atomic<bool> running{false};
    std::string agent_id;
    std::string agent_name;
    std::string agent_type;
    std::vector<std::string> capabilities;
    
    // Instance mutexes to prevent deadlocks
    mutable std::mutex capabilities_mutex;
    mutable std::mutex message_mutex;

public:
    AgentCore(const std::string& name = "", const std::string& type = "generic");
    ~AgentCore();

    // Lifecycle management
    void start();
    void stop();
    bool is_running() const { return running.load(); }

    // Configuration
    void set_message_router(std::shared_ptr<MessageRouter> router);
    void add_capability(const std::string& capability);

    // Function execution
    FunctionResult execute_function(const std::string& name, const AgentData& params);
    std::string execute_function_async(const std::string& name, const AgentData& params, int priority = 0);

    // Messaging
    void send_message(const std::string& to_agent, const std::string& message_type, 
                     const AgentData& payload = AgentData());
    void broadcast_message(const std::string& message_type, const AgentData& payload = AgentData());

    // Getters
    const std::string& get_agent_id() const { return agent_id; }
    const std::string& get_agent_name() const { return agent_name; }
    const std::string& get_agent_type() const { return agent_type; }
    const std::vector<std::string>& get_capabilities() const { return capabilities; }
    
    std::shared_ptr<Logger> get_logger() { return logger; }
    std::shared_ptr<FunctionManager> get_function_manager() { return function_manager; }
    std::shared_ptr<JobManager> get_job_manager() { return job_manager; }
    std::shared_ptr<EventSystem> get_event_system() { return event_system; }

private:
    void handle_message(const AgentMessage& message);
};

} // namespace kolosal::agents
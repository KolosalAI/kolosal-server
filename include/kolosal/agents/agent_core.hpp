// File: include/kolosal/agents/agent_core.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include "agent_roles.hpp"
#include "function_manager.hpp"
#include "job_manager.hpp"
#include "event_system.hpp"
#include "message_router.hpp"
#include "tool_registry.hpp"
#include "memory_manager.hpp"
#include "planning_system.hpp"
#include <memory>
#include <atomic>
#include <vector>
#include <mutex>

namespace kolosal::agents {

// Forward declaration
class Logger;

/**
 * @brief Core agent implementation with advanced capabilities
 */
class KOLOSAL_SERVER_API AgentCore : public std::enable_shared_from_this<AgentCore> {
private:
    std::shared_ptr<Logger> logger;
    std::shared_ptr<FunctionManager> function_manager;
    std::shared_ptr<JobManager> job_manager;
    std::shared_ptr<EventSystem> event_system;
    std::shared_ptr<MessageRouter> message_router;
    std::shared_ptr<ToolRegistry> tool_registry;
    std::shared_ptr<MemoryManager> memory_manager;
    std::shared_ptr<PlanningReasoningCoordinator> planning_coordinator;
    std::shared_ptr<AgentRoleManager> role_manager;
    
    std::atomic<bool> running{false};
    std::string agent_id;
    std::string agent_name;
    std::string agent_type;
    AgentRole current_role;
    std::vector<AgentSpecialization> specializations;
    std::vector<std::string> capabilities;
    
    // Instance mutexes to prevent deadlocks
    mutable std::mutex capabilities_mutex;
    mutable std::mutex message_mutex;

public:
    AgentCore(const std::string& name = "", const std::string& type = "generic", 
             AgentRole role = AgentRole::GENERIC);
    ~AgentCore();

    // Lifecycle management
    void start();
    void stop();
    bool is_running() const { return running.load(); }

    // Role and capability management
    void set_role(AgentRole role);
    AgentRole get_role() const { return current_role; }
    void add_specialization(AgentSpecialization spec);
    std::vector<AgentSpecialization> get_specializations() const { return specializations; }
    void set_message_router(std::shared_ptr<MessageRouter> router);
    void add_capability(const std::string& capability);

    // Enhanced function and tool execution
    FunctionResult execute_function(const std::string& name, const AgentData& params);
    std::string execute_function_async(const std::string& name, const AgentData& params, int priority = 0);
    FunctionResult execute_tool(const std::string& tool_name, const AgentData& params);
    
    // Planning and reasoning
    ExecutionPlan create_plan(const std::string& goal, const std::string& context = "");
    bool execute_plan(const std::string& plan_id);
    std::string reason_about(const std::string& question, const std::string& context = "");
    
    // Memory management
    void store_memory(const std::string& content, const std::string& type = "general");
    std::vector<MemoryEntry> recall_memories(const std::string& query, int max_results = 5);
    void set_working_context(const std::string& key, const AgentData& data);
    AgentData get_working_context(const std::string& key);

    // Messaging
    void send_message(const std::string& to_agent, const std::string& message_type, 
                     const AgentData& payload = AgentData());
    void broadcast_message(const std::string& message_type, const AgentData& payload = AgentData());

    // Tool discovery and management
    std::vector<std::string> discover_tools(const ToolFilter& filter = ToolFilter());
    bool register_custom_tool(std::unique_ptr<Tool> tool);
    ToolSchema get_tool_schema(const std::string& tool_name);

    // Getters
    const std::string& get_agent_id() const { return agent_id; }
    const std::string& get_agent_name() const { return agent_name; }
    const std::string& get_agent_type() const { return agent_type; }
    const std::vector<std::string>& get_capabilities() const { return capabilities; }
    
    // Component access
    std::shared_ptr<Logger> get_logger() { return logger; }
    std::shared_ptr<FunctionManager> get_function_manager() { return function_manager; }
    std::shared_ptr<JobManager> get_job_manager() { return job_manager; }
    std::shared_ptr<EventSystem> get_event_system() { return event_system; }
    std::shared_ptr<ToolRegistry> get_tool_registry() { return tool_registry; }
    std::shared_ptr<MemoryManager> get_memory_manager() { return memory_manager; }
    std::shared_ptr<PlanningReasoningCoordinator> get_planning_coordinator() { return planning_coordinator; }
    
    // Performance and monitoring
    struct AgentStats {
        size_t total_functions_executed;
        size_t total_tools_executed;
        size_t total_plans_created;
        size_t memory_entries_count;
        double average_execution_time_ms;
        std::chrono::time_point<std::chrono::system_clock> last_activity;
    };
    
    AgentStats get_statistics() const;

private:
    void handle_message(const AgentMessage& message);
};

} // namespace kolosal::agents
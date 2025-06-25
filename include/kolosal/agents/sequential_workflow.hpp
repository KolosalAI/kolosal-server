#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include "multi_agent_system.hpp"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace kolosal::agents {

// Forward declarations
class Logger;
class YAMLConfigurableAgentManager;

/**
 * @brief Sequential workflow step definition
 */
struct SequentialWorkflowStep {
    std::string step_id;
    std::string step_name;
    std::string description;
    std::string agent_id;
    std::string function_name;
    AgentData parameters;
    
    // Step configuration
    int timeout_seconds = 60;
    int max_retries = 3;
    bool continue_on_failure = false;
    bool skip_on_condition = false;
    
    // Step validation
    std::function<bool(const AgentData&)> precondition;
    std::function<bool(const FunctionResult&)> validation;
    std::function<AgentData(const AgentData&, const FunctionResult&)> result_processor;
    
    // Step metadata
    std::map<std::string, std::string> metadata;
    int priority = 0;
    
    SequentialWorkflowStep() = default;
    SequentialWorkflowStep(const std::string& id, const std::string& name, 
                          const std::string& agent, const std::string& function)
        : step_id(id), step_name(name), agent_id(agent), function_name(function) {}
};

/**
 * @brief Sequential workflow execution result
 */
struct SequentialWorkflowResult {
    std::string workflow_id;
    std::string workflow_name;
    bool success = false;
    std::string error_message;
    
    // Step results
    std::vector<std::string> executed_steps;
    std::map<std::string, FunctionResult> step_results;
    std::map<std::string, std::string> step_errors;
    std::map<std::string, double> step_execution_times;
    
    // Workflow timing
    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> end_time;
    double total_execution_time_ms = 0.0;
    
    // Workflow context
    AgentData final_context;
    AgentData initial_context;
    
    // Execution metadata
    int total_steps = 0;
    int successful_steps = 0;
    int failed_steps = 0;
    int skipped_steps = 0;
    int retried_steps = 0;
};

/**
 * @brief Sequential workflow definition
 */
struct SequentialWorkflow {
    std::string workflow_id;
    std::string workflow_name;
    std::string description;
    std::vector<SequentialWorkflowStep> steps;
    
    // Workflow configuration
    AgentData global_context;
    bool stop_on_failure = true;
    bool save_intermediate_results = true;
    int max_execution_time_seconds = 600; // 10 minutes
    
    // Workflow callbacks
    std::function<void(const std::string&, const FunctionResult&)> on_step_complete;
    std::function<void(const std::string&, const std::string&)> on_step_error;
    std::function<void(const SequentialWorkflowResult&)> on_workflow_complete;
    
    // Workflow metadata
    std::map<std::string, std::string> metadata;
    std::string created_by;
    std::chrono::time_point<std::chrono::system_clock> created_at;
    
    SequentialWorkflow() : created_at(std::chrono::system_clock::now()) {}
    SequentialWorkflow(const std::string& id, const std::string& name)
        : workflow_id(id), workflow_name(name), created_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief Sequential workflow executor
 */
class KOLOSAL_SERVER_API SequentialWorkflowExecutor {
private:
    std::shared_ptr<YAMLConfigurableAgentManager> agent_manager;
    std::shared_ptr<Logger> logger;
    
    // Workflow management
    std::unordered_map<std::string, SequentialWorkflow> workflows;
    std::unordered_map<std::string, SequentialWorkflowResult> workflow_results;
    mutable std::mutex cancellation_flags_mutex;
    std::unordered_map<std::string, std::unique_ptr<std::atomic<bool>>> workflow_cancellation_flags;
    
    // Thread safety
    mutable std::mutex workflow_mutex;
    mutable std::mutex results_mutex;
    
    // Execution state
    std::atomic<int> active_workflows{0};
    std::atomic<int> completed_workflows{0};
    std::atomic<int> failed_workflows{0};

public:
    explicit SequentialWorkflowExecutor(std::shared_ptr<YAMLConfigurableAgentManager> manager);
    ~SequentialWorkflowExecutor();
    
    // Workflow management
    bool register_workflow(const SequentialWorkflow& workflow);
    bool remove_workflow(const std::string& workflow_id);
    std::vector<std::string> list_workflows() const;
    std::optional<SequentialWorkflow> get_workflow(const std::string& workflow_id) const;
    
    // Workflow execution
    SequentialWorkflowResult execute_workflow(const std::string& workflow_id, 
                                            const AgentData& input_context = AgentData());
    std::string execute_workflow_async(const std::string& workflow_id, 
                                      const AgentData& input_context = AgentData());
    
    // Workflow control
    bool cancel_workflow(const std::string& workflow_id);
    bool pause_workflow(const std::string& workflow_id);
    bool resume_workflow(const std::string& workflow_id);
    
    // Results and monitoring
    std::optional<SequentialWorkflowResult> get_workflow_result(const std::string& workflow_id) const;
    std::map<std::string, std::string> get_workflow_status(const std::string& workflow_id) const;
    std::map<std::string, int> get_executor_metrics() const;
    
    // Workflow building helpers
    SequentialWorkflow create_workflow(const std::string& workflow_id, const std::string& name);
    SequentialWorkflowStep create_step(const std::string& step_id, const std::string& step_name,
                                      const std::string& agent_id, const std::string& function_name);
    
    // Advanced features
    bool validate_workflow(const SequentialWorkflow& workflow) const;
    std::vector<std::string> get_workflow_dependencies(const std::string& workflow_id) const;
    bool export_workflow_template(const std::string& workflow_id, const std::string& file_path) const;
    bool import_workflow_template(const std::string& file_path);

private:
    // Internal execution methods
    SequentialWorkflowResult execute_workflow_internal(const SequentialWorkflow& workflow, 
                                                      const AgentData& input_context);
    bool execute_step(const SequentialWorkflowStep& step, AgentData& context, 
                     FunctionResult& result, std::string& error_message);
    bool validate_step_precondition(const SequentialWorkflowStep& step, const AgentData& context);
    bool validate_step_result(const SequentialWorkflowStep& step, const FunctionResult& result);
    AgentData process_step_result(const SequentialWorkflowStep& step, const AgentData& context, 
                                 const FunctionResult& result);
    
    // Utility methods
    void log_step_execution(const std::string& workflow_id, const std::string& step_id, 
                           const FunctionResult& result, double execution_time);
    void update_workflow_metrics(const SequentialWorkflowResult& result);
    bool check_workflow_timeout(const std::chrono::time_point<std::chrono::system_clock>& start_time,
                               int max_seconds);
};

/**
 * @brief Workflow builder for easy workflow creation
 */
class KOLOSAL_SERVER_API SequentialWorkflowBuilder {
private:
    SequentialWorkflow workflow;
    
public:
    explicit SequentialWorkflowBuilder(const std::string& workflow_id, const std::string& name);
    
    // Basic configuration
    SequentialWorkflowBuilder& description(const std::string& desc);
    SequentialWorkflowBuilder& global_context(const AgentData& context);
    SequentialWorkflowBuilder& stop_on_failure(bool stop);
    SequentialWorkflowBuilder& max_execution_time(int seconds);
    SequentialWorkflowBuilder& metadata(const std::string& key, const std::string& value);
    
    // Step addition
    SequentialWorkflowBuilder& add_step(const SequentialWorkflowStep& step);
    SequentialWorkflowBuilder& add_step(const std::string& step_id, const std::string& step_name,
                                       const std::string& agent_id, const std::string& function_name);
    SequentialWorkflowBuilder& add_step(const std::string& step_id, const std::string& step_name,
                                       const std::string& agent_id, const std::string& function_name,
                                       const AgentData& parameters);
    
    // Step configuration
    SequentialWorkflowBuilder& step_timeout(int seconds);
    SequentialWorkflowBuilder& step_retries(int retries);
    SequentialWorkflowBuilder& step_continue_on_failure(bool continue_on_fail);
    SequentialWorkflowBuilder& step_precondition(std::function<bool(const AgentData&)> condition);
    SequentialWorkflowBuilder& step_validation(std::function<bool(const FunctionResult&)> validation);
    SequentialWorkflowBuilder& step_processor(std::function<AgentData(const AgentData&, const FunctionResult&)> processor);
    
    // Callbacks
    SequentialWorkflowBuilder& on_step_complete(std::function<void(const std::string&, const FunctionResult&)> callback);
    SequentialWorkflowBuilder& on_step_error(std::function<void(const std::string&, const std::string&)> callback);
    SequentialWorkflowBuilder& on_workflow_complete(std::function<void(const SequentialWorkflowResult&)> callback);
    
    // Build
    SequentialWorkflow build();
};

} // namespace kolosal::agents

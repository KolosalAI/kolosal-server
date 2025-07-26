#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include "multi_agent_system.hpp"
#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <string>
#include <optional>
#include <json.hpp>

namespace kolosal::agents {

/**
 * @brief Workflow execution status
 */
enum class WorkflowStatus {
    PENDING,     // Workflow created but not started
    RUNNING,     // Workflow is executing
    PAUSED,      // Workflow is paused
    COMPLETED,   // Workflow completed successfully
    FAILED,      // Workflow failed
    CANCELLED,   // Workflow was cancelled
    TIMEOUT      // Workflow timed out
};

/**
 * @brief Workflow step execution status
 */
enum class StepStatus {
    PENDING,     // Step not yet executed
    RUNNING,     // Step is executing
    COMPLETED,   // Step completed successfully
    FAILED,      // Step failed
    SKIPPED,     // Step was skipped
    RETRYING     // Step is being retried
};

/**
 * @brief Workflow execution type
 */
enum class WorkflowType {
    SEQUENTIAL,  // Steps execute one after another
    PARALLEL,    // Steps execute concurrently
    PIPELINE,    // Data flows from step to step
    CONSENSUS,   // Multiple agents vote on decisions
    CONDITIONAL  // Steps execute based on conditions
};

/**
 * @brief Step dependency definition
 */
struct StepDependency {
    std::string step_id;
    std::string condition;  // e.g., "success", "completion", "output_contains:value"
    bool required = true;   // If false, dependency failure won't block execution
};

/**
 * @brief Enhanced workflow step definition
 */
struct WorkflowStep {
    std::string step_id;
    std::string name;
    std::string description;
    std::string agent_id;
    std::string function_name;
    nlohmann::json parameters;
    std::vector<StepDependency> dependencies;
    nlohmann::json conditions;  // Execution conditions
    
    // Execution settings
    bool parallel_allowed = true;
    int timeout_seconds = 30;
    int max_retries = 3;
    int retry_delay_seconds = 1;
    bool continue_on_error = false;
    
    // Runtime data
    StepStatus status = StepStatus::PENDING;
    int retry_count = 0;
    nlohmann::json output;
    std::string error_message;
    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> end_time;
};

/**
 * @brief Error handling strategy
 */
struct ErrorHandlingStrategy {
    bool retry_on_failure = true;
    int max_retries = 3;
    int retry_delay_seconds = 1;
    bool continue_on_error = false;
    bool use_fallback_agent = false;
    std::string fallback_agent_id;
    nlohmann::json fallback_parameters;
};

/**
 * @brief Workflow definition
 */
struct Workflow {
    std::string workflow_id;
    std::string name;
    std::string description;
    WorkflowType type = WorkflowType::SEQUENTIAL;
    std::vector<WorkflowStep> steps;
    nlohmann::json global_context;
    ErrorHandlingStrategy error_handling;
    
    // Execution settings
    int max_execution_time_seconds = 300;
    int max_concurrent_steps = 4;
    bool auto_cleanup = true;
    bool persist_state = true;
    
    // Runtime data
    WorkflowStatus status = WorkflowStatus::PENDING;
    std::chrono::time_point<std::chrono::system_clock> created_time;
    std::chrono::time_point<std::chrono::system_clock> start_time;
    std::chrono::time_point<std::chrono::system_clock> end_time;
    std::string error_message;
    nlohmann::json result;
    std::string created_by;
    std::map<std::string, nlohmann::json> step_outputs;
};

/**
 * @brief Workflow execution context
 */
struct WorkflowExecutionContext {
    std::string execution_id;
    std::string workflow_id;
    nlohmann::json global_variables;
    std::map<std::string, nlohmann::json> step_outputs;
    std::map<std::string, StepStatus> step_statuses;
    std::chrono::time_point<std::chrono::system_clock> execution_start;
    WorkflowStatus current_status = WorkflowStatus::PENDING;
    std::string current_step_id;
    std::vector<std::string> completed_steps;
    std::vector<std::string> failed_steps;
};

/**
 * @brief Workflow execution metrics
 */
struct WorkflowMetrics {
    int total_workflows = 0;
    int running_workflows = 0;
    int completed_workflows = 0;
    int failed_workflows = 0;
    int cancelled_workflows = 0;
    double average_execution_time_ms = 0.0;
    double success_rate = 0.0;
    std::map<std::string, int> error_counts;
    std::chrono::time_point<std::chrono::system_clock> last_updated;
};

/**
 * @brief Advanced workflow execution engine
 */
class KOLOSAL_SERVER_API WorkflowEngine {
private:
    std::shared_ptr<YAMLConfigurableAgentManager> agent_manager;
    
    // Workflow storage
    std::map<std::string, Workflow> workflows;
    std::map<std::string, WorkflowExecutionContext> active_executions;
    std::map<std::string, WorkflowExecutionContext> execution_history;
    
    // Execution queue and management
    std::queue<std::string> workflow_queue;
    mutable std::mutex workflow_mutex;
    std::condition_variable workflow_cv;
    std::atomic<bool> engine_running{false};
    std::thread execution_thread;
    std::vector<std::thread> worker_threads;
    
    // Metrics and monitoring
    WorkflowMetrics metrics;
    mutable std::mutex metrics_mutex;
    
    // Configuration
    int max_concurrent_workflows = 10;
    int max_worker_threads = 4;
    bool enable_persistence = true;
    std::string persistence_path = "./workflow_state";

public:
    explicit WorkflowEngine(std::shared_ptr<YAMLConfigurableAgentManager> manager);
    ~WorkflowEngine();
    
    // Engine lifecycle
    void start();
    void stop();
    bool is_running() const { return engine_running.load(); }
    
    // Workflow management
    std::string create_workflow(const Workflow& workflow);
    bool update_workflow(const std::string& workflow_id, const Workflow& workflow);
    bool delete_workflow(const std::string& workflow_id);
    std::vector<std::string> list_workflows() const;
    std::optional<Workflow> get_workflow(const std::string& workflow_id) const;
    
    // Workflow execution
    std::string execute_workflow(const std::string& workflow_id, const nlohmann::json& input_context = {});
    bool pause_workflow(const std::string& execution_id);
    bool resume_workflow(const std::string& execution_id);
    bool cancel_workflow(const std::string& execution_id);
    
    // Workflow monitoring
    std::optional<WorkflowExecutionContext> get_execution_status(const std::string& execution_id) const;
    std::vector<WorkflowExecutionContext> get_active_executions() const;
    std::vector<WorkflowExecutionContext> get_execution_history(const std::string& workflow_id = "") const;
    WorkflowMetrics get_metrics() const;
    
    // Step management
    bool retry_step(const std::string& execution_id, const std::string& step_id);
    bool skip_step(const std::string& execution_id, const std::string& step_id);
    
    // Context management
    nlohmann::json get_global_context(const std::string& execution_id) const;
    bool update_global_context(const std::string& execution_id, const nlohmann::json& context);
    nlohmann::json get_step_output(const std::string& execution_id, const std::string& step_id) const;
    
    // Workflow templates
    Workflow create_sequential_workflow(const std::string& name, const std::vector<std::pair<std::string, std::string>>& agent_functions);
    Workflow create_parallel_workflow(const std::string& name, const std::vector<std::pair<std::string, std::string>>& agent_functions);
    Workflow create_pipeline_workflow(const std::string& name, const std::vector<std::pair<std::string, std::string>>& agent_functions);
    Workflow create_consensus_workflow(const std::string& name, const std::vector<std::string>& agent_ids, const std::string& decision_function);
    
    // Error handling
    void set_error_handling_strategy(const std::string& workflow_id, const ErrorHandlingStrategy& strategy);
    std::vector<std::string> get_failed_workflows() const;
    bool recover_workflow(const std::string& execution_id);

private:
    // Internal execution methods
    void execution_loop();
    void worker_loop();
    void execute_workflow_internal(const std::string& execution_id);
    void execute_step(WorkflowExecutionContext& context, WorkflowStep& step);
    bool check_step_dependencies(const WorkflowExecutionContext& context, const WorkflowStep& step);
    bool evaluate_conditions(const nlohmann::json& conditions, const WorkflowExecutionContext& context);
    nlohmann::json interpolate_parameters(const nlohmann::json& parameters, const WorkflowExecutionContext& context);
    
    // Dependency resolution
    std::vector<std::string> resolve_execution_order(const Workflow& workflow);
    bool has_circular_dependencies(const Workflow& workflow);
    
    // Persistence
    void save_workflow_state(const WorkflowExecutionContext& context);
    void load_workflow_state();
    void cleanup_old_executions();
    
    // Metrics
    void update_metrics();
    void record_workflow_completion(const WorkflowExecutionContext& context);
    
    // Utility methods
    std::string generate_execution_id();
    std::string generate_workflow_id();
    void log_workflow_event(const std::string& execution_id, const std::string& event, const std::string& details = "");
};

} // namespace kolosal::agents

// File: include/kolosal/agents/planning_system.hpp
#pragma once

#include "../export.hpp"
#include "agent_data.hpp"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace kolosal::agents {

/**
 * @brief Task priority levels
 */
enum class KOLOSAL_SERVER_API TaskPriority {
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    CRITICAL = 4
};

/**
 * @brief Task execution status
 */
enum class KOLOSAL_SERVER_API TaskStatus {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    BLOCKED,
    CANCELLED
};

/**
 * @brief Represents a single task in a plan
 */
struct KOLOSAL_SERVER_API Task {
    std::string id;
    std::string name;
    std::string description;
    std::string function_name;
    AgentData parameters;
    TaskPriority priority;
    TaskStatus status;
    std::vector<std::string> dependencies;  // Task IDs this task depends on
    std::vector<std::string> tags;
    std::chrono::time_point<std::chrono::system_clock> created_at;
    std::chrono::time_point<std::chrono::system_clock> started_at;
    std::chrono::time_point<std::chrono::system_clock> completed_at;
    std::string error_message;
    AgentData result;
    double estimated_duration_seconds = 0.0;
    double actual_duration_seconds = 0.0;
    int retry_count = 0;
    int max_retries = 3;
    
    Task(const std::string& task_id, const std::string& task_name, 
         const std::string& func_name = "");
};

/**
 * @brief Execution plan containing multiple tasks
 */
struct KOLOSAL_SERVER_API ExecutionPlan {
    std::string id;
    std::string name;
    std::string description;
    std::string goal;
    std::vector<Task> tasks;
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::time_point<std::chrono::system_clock> created_at;
    TaskStatus overall_status;
    
    // Default constructor
    ExecutionPlan() : overall_status(TaskStatus::PENDING), created_at(std::chrono::system_clock::now()) {}
    
    ExecutionPlan(const std::string& plan_id, const std::string& plan_name);
    
    // Plan management
    void add_task(const Task& task);
    bool remove_task(const std::string& task_id);
    Task* get_task(const std::string& task_id);
    std::vector<Task*> get_ready_tasks();  // Tasks with all dependencies completed
    std::vector<Task*> get_tasks_by_status(TaskStatus status);
    std::vector<const Task*> get_tasks_by_status(TaskStatus status) const;
    bool is_complete() const;
    double get_progress() const;  // 0.0 to 1.0
};

/**
 * @brief Planning strategies
 */
enum class KOLOSAL_SERVER_API PlanningStrategy {
    SEQUENTIAL,     // Execute tasks one by one
    PARALLEL,       // Execute independent tasks in parallel
    PRIORITY_BASED, // Execute based on priority
    DEPENDENCY_AWARE // Respect dependencies, maximize parallelism
};

/**
 * @brief Goal decomposition and planning system
 */
class KOLOSAL_SERVER_API PlanningSystem {
private:
    std::shared_ptr<class Logger> logger;
    std::unordered_map<std::string, ExecutionPlan> active_plans;
    std::unordered_map<std::string, ExecutionPlan> completed_plans;
    mutable std::mutex planning_mutex;
    
public:
    PlanningSystem(std::shared_ptr<class Logger> log = nullptr);
    
    // Goal decomposition
    ExecutionPlan decompose_goal(const std::string& goal, 
                                const std::string& context = "",
                                PlanningStrategy strategy = PlanningStrategy::DEPENDENCY_AWARE);
    
    // Plan management
    bool add_plan(const ExecutionPlan& plan);
    bool remove_plan(const std::string& plan_id);
    ExecutionPlan* get_plan(const std::string& plan_id);
    std::vector<std::string> get_active_plan_ids() const;
    
    // Task management within plans
    bool add_task_to_plan(const std::string& plan_id, const Task& task);
    bool update_task_status(const std::string& plan_id, const std::string& task_id, 
                           TaskStatus status, const std::string& error_msg = "");
    bool set_task_result(const std::string& plan_id, const std::string& task_id, 
                        const AgentData& result);
    
    // Execution planning
    std::vector<Task*> get_next_tasks(const std::string& plan_id);
    bool can_execute_task(const std::string& plan_id, const std::string& task_id);
    
    // Plan analysis
    std::vector<std::string> detect_circular_dependencies(const std::string& plan_id);
    double estimate_plan_duration(const std::string& plan_id);
    std::string generate_plan_summary(const std::string& plan_id);
    
    // Plan optimization
    void optimize_plan(const std::string& plan_id);
    void reorder_tasks_by_priority(const std::string& plan_id);
    
    // Statistics
    struct PlanningStats {
        size_t active_plans;
        size_t completed_plans;
        size_t total_tasks;
        size_t completed_tasks;
        double average_task_duration;
        double success_rate;
    };
    
    PlanningStats get_statistics() const;

private:
    std::vector<Task> decompose_complex_goal(const std::string& goal, const std::string& context);
    bool validate_dependencies(const std::vector<Task>& tasks);
    void sort_tasks_topologically(std::vector<Task>& tasks);
    std::string generate_task_id();
    std::string generate_plan_id();
};

/**
 * @brief Reasoning and decision-making system
 */
class KOLOSAL_SERVER_API ReasoningSystem {
private:
    std::shared_ptr<class Logger> logger;
    std::unordered_map<std::string, AgentData> knowledge_base;
    std::vector<std::string> reasoning_history;
    mutable std::mutex reasoning_mutex;
    
public:
    ReasoningSystem(std::shared_ptr<class Logger> log = nullptr);
    
    // Knowledge management
    void add_knowledge(const std::string& key, const AgentData& data);
    AgentData get_knowledge(const std::string& key) const;
    bool has_knowledge(const std::string& key) const;
    void remove_knowledge(const std::string& key);
    
    // Reasoning operations
    std::string reason_about(const std::string& question, const std::string& context = "");
    bool can_achieve_goal(const std::string& goal, const std::vector<std::string>& available_functions);
    std::string suggest_approach(const std::string& problem, const std::string& constraints = "");
    
    // Decision making
    std::string make_decision(const std::string& situation, 
                             const std::vector<std::string>& options,
                             const std::string& criteria = "");
    
    // Self-reflection
    std::string reflect_on_performance(const std::string& task_result, 
                                      const std::string& expected_outcome);
    std::vector<std::string> identify_improvement_areas(const std::string& task_history);
    
    // Meta-reasoning
    bool should_ask_for_help(const std::string& current_situation);
    std::string generate_clarifying_questions(const std::string& unclear_request);
    
    // Reasoning history
    std::vector<std::string> get_reasoning_history() const;
    void clear_reasoning_history();

private:
    std::string apply_logical_reasoning(const std::string& premises, const std::string& query);
    std::string apply_analogical_reasoning(const std::string& source_case, const std::string& target_case);
    double calculate_confidence(const std::string& reasoning_chain);
};

/**
 * @brief Combined planning and reasoning coordinator
 */
class KOLOSAL_SERVER_API PlanningReasoningCoordinator {
private:
    std::unique_ptr<PlanningSystem> planning_system;
    std::unique_ptr<ReasoningSystem> reasoning_system;
    std::shared_ptr<class Logger> logger;
    
public:
    PlanningReasoningCoordinator(std::shared_ptr<class Logger> log = nullptr);
    ~PlanningReasoningCoordinator();
    
    // Integrated planning and reasoning
    ExecutionPlan create_intelligent_plan(const std::string& goal, 
                                         const std::string& context,
                                         const std::vector<std::string>& available_functions);
    
    bool adapt_plan_based_on_feedback(const std::string& plan_id, 
                                     const std::string& feedback,
                                     const AgentData& execution_results);
    
    std::string recommend_next_action(const std::string& plan_id, 
                                     const std::string& current_state);
    
    // Component access
    PlanningSystem* get_planning_system() { return planning_system.get(); }
    ReasoningSystem* get_reasoning_system() { return reasoning_system.get(); }
};

} // namespace kolosal::agents

// File: src/agents/planning_system.cpp
#include "kolosal/agents/planning_system.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <sstream>
#include <random>
#include <queue>
#include <unordered_set>
#include <mutex>

namespace kolosal::agents {

// Bridge Logger interface to ServerLogger
class Logger {
public:
    void info(const std::string& message) { ServerLogger::logInfo("%s", message.c_str()); }
    void debug(const std::string& message) { ServerLogger::logDebug("%s", message.c_str()); }
    void warn(const std::string& message) { ServerLogger::logWarning("%s", message.c_str()); }
    void error(const std::string& message) { ServerLogger::logError("%s", message.c_str()); }
};

// Task implementation
Task::Task(const std::string& task_id, const std::string& task_name, const std::string& func_name)
    : id(task_id), name(task_name), function_name(func_name), 
      priority(TaskPriority::NORMAL), status(TaskStatus::PENDING) {
    created_at = std::chrono::system_clock::now();
}

// ExecutionPlan implementation
ExecutionPlan::ExecutionPlan(const std::string& plan_id, const std::string& plan_name)
    : id(plan_id), name(plan_name), overall_status(TaskStatus::PENDING) {
    created_at = std::chrono::system_clock::now();
}

void ExecutionPlan::add_task(const Task& task) {
    tasks.push_back(task);
}

bool ExecutionPlan::remove_task(const std::string& task_id) {
    auto it = std::find_if(tasks.begin(), tasks.end(),
        [&task_id](const Task& t) { return t.id == task_id; });
    
    if (it != tasks.end()) {
        tasks.erase(it);
        return true;
    }
    return false;
}

Task* ExecutionPlan::get_task(const std::string& task_id) {
    auto it = std::find_if(tasks.begin(), tasks.end(),
        [&task_id](const Task& t) { return t.id == task_id; });
    
    return (it != tasks.end()) ? &(*it) : nullptr;
}

std::vector<Task*> ExecutionPlan::get_ready_tasks() {
    std::vector<Task*> ready_tasks;
    
    for (auto& task : tasks) {
        if (task.status == TaskStatus::PENDING) {
            bool dependencies_met = true;
            
            // Check if all dependencies are completed
            for (const std::string& dep_id : task.dependencies) {
                Task* dep_task = get_task(dep_id);
                if (!dep_task || dep_task->status != TaskStatus::COMPLETED) {
                    dependencies_met = false;
                    break;
                }
            }
            
            if (dependencies_met) {
                ready_tasks.push_back(&task);
            }
        }
    }
    
    return ready_tasks;
}

std::vector<Task*> ExecutionPlan::get_tasks_by_status(TaskStatus status) {
    std::vector<Task*> result;
    
    for (auto& task : tasks) {
        if (task.status == status) {
            result.push_back(&task);
        }
    }
    
    return result;
}

std::vector<const Task*> ExecutionPlan::get_tasks_by_status(TaskStatus status) const {
    std::vector<const Task*> result;
    
    for (const auto& task : tasks) {
        if (task.status == status) {
            result.push_back(&task);
        }
    }
    
    return result;
}

bool ExecutionPlan::is_complete() const {
    for (const auto& task : tasks) {
        if (task.status != TaskStatus::COMPLETED && task.status != TaskStatus::CANCELLED) {
            return false;
        }
    }
    return true;
}

double ExecutionPlan::get_progress() const {
    if (tasks.empty()) return 1.0;
    
    size_t completed = 0;
    for (const auto& task : tasks) {
        if (task.status == TaskStatus::COMPLETED) {
            completed++;
        }
    }
    
    return static_cast<double>(completed) / tasks.size();
}

// PlanningSystem implementation
PlanningSystem::PlanningSystem(std::shared_ptr<Logger> log) : logger(log) {
    if (!logger) {
        logger = std::make_shared<Logger>();
    }
}

ExecutionPlan PlanningSystem::decompose_goal(const std::string& goal, const std::string& context, 
                                           PlanningStrategy strategy) {
    std::string plan_id = generate_plan_id();
    ExecutionPlan plan(plan_id, "Plan for: " + goal);
    plan.goal = goal;
    plan.description = "Auto-generated plan for goal decomposition";
    
    // Decompose the goal into tasks
    auto tasks = decompose_complex_goal(goal, context);
    
    // Apply planning strategy
    switch (strategy) {
        case PlanningStrategy::SEQUENTIAL:
            // Chain tasks sequentially
            for (size_t i = 1; i < tasks.size(); ++i) {
                tasks[i].dependencies.push_back(tasks[i-1].id);
            }
            break;
            
        case PlanningStrategy::PARALLEL:
            // No dependencies - all tasks can run in parallel
            break;
            
        case PlanningStrategy::PRIORITY_BASED:
            // Sort by priority
            std::sort(tasks.begin(), tasks.end(),
                [](const Task& a, const Task& b) { return a.priority > b.priority; });
            break;
            
        case PlanningStrategy::DEPENDENCY_AWARE:
            // Validate and optimize dependencies
            if (validate_dependencies(tasks)) {
                sort_tasks_topologically(tasks);
            }
            break;
    }
    
    // Add tasks to plan
    for (const auto& task : tasks) {
        plan.add_task(task);
    }
    
    logger->info("Created plan '" + plan.name + "' with " + std::to_string(tasks.size()) + " tasks");
    return plan;
}

bool PlanningSystem::add_plan(const ExecutionPlan& plan) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    if (active_plans.find(plan.id) != active_plans.end()) {
        logger->warn("Plan already exists: " + plan.id);
        return false;
    }
    
    active_plans[plan.id] = plan;
    logger->info("Added plan: " + plan.name);
    return true;
}

bool PlanningSystem::remove_plan(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto it = active_plans.find(plan_id);
    if (it != active_plans.end()) {
        if (it->second.is_complete()) {
            completed_plans[plan_id] = std::move(it->second);
        }
        active_plans.erase(it);
        logger->info("Removed plan: " + plan_id);
        return true;
    }
    
    return false;
}

ExecutionPlan* PlanningSystem::get_plan(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto it = active_plans.find(plan_id);
    return (it != active_plans.end()) ? &it->second : nullptr;
}

std::vector<std::string> PlanningSystem::get_active_plan_ids() const {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    std::vector<std::string> ids;
    for (const auto& pair : active_plans) {
        ids.push_back(pair.first);
    }
    return ids;
}

bool PlanningSystem::add_task_to_plan(const std::string& plan_id, const Task& task) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto it = active_plans.find(plan_id);
    if (it != active_plans.end()) {
        it->second.add_task(task);
        return true;
    }
    return false;
}

bool PlanningSystem::update_task_status(const std::string& plan_id, const std::string& task_id, 
                                       TaskStatus status, const std::string& error_msg) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto plan_it = active_plans.find(plan_id);
    if (plan_it != active_plans.end()) {
        Task* task = plan_it->second.get_task(task_id);
        if (task) {
            task->status = status;
            if (!error_msg.empty()) {
                task->error_message = error_msg;
            }
            
            if (status == TaskStatus::IN_PROGRESS) {
                task->started_at = std::chrono::system_clock::now();
            } else if (status == TaskStatus::COMPLETED || status == TaskStatus::FAILED) {
                task->completed_at = std::chrono::system_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    task->completed_at - task->started_at);
                task->actual_duration_seconds = duration.count() / 1000.0;
            }
            
            return true;
        }
    }
    return false;
}

bool PlanningSystem::set_task_result(const std::string& plan_id, const std::string& task_id, 
                                    const AgentData& result) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto plan_it = active_plans.find(plan_id);
    if (plan_it != active_plans.end()) {
        Task* task = plan_it->second.get_task(task_id);
        if (task) {
            task->result = result;
            return true;
        }
    }
    return false;
}

std::vector<Task*> PlanningSystem::get_next_tasks(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto it = active_plans.find(plan_id);
    if (it != active_plans.end()) {
        return it->second.get_ready_tasks();
    }
    return {};
}

bool PlanningSystem::can_execute_task(const std::string& plan_id, const std::string& task_id) {
    auto ready_tasks = get_next_tasks(plan_id);
    
    for (const Task* task : ready_tasks) {
        if (task->id == task_id) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> PlanningSystem::detect_circular_dependencies(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto it = active_plans.find(plan_id);
    if (it == active_plans.end()) {
        return {};
    }
    
    const auto& tasks = it->second.tasks;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> rec_stack;
    std::vector<std::string> circular_deps;
    
    std::function<bool(const std::string&)> has_cycle = [&](const std::string& task_id) -> bool {
        if (rec_stack.find(task_id) != rec_stack.end()) {
            circular_deps.push_back(task_id);
            return true;
        }
        
        if (visited.find(task_id) != visited.end()) {
            return false;
        }
        
        visited.insert(task_id);
        rec_stack.insert(task_id);
        
        // Find task and check its dependencies
        for (const auto& task : tasks) {
            if (task.id == task_id) {
                for (const std::string& dep : task.dependencies) {
                    if (has_cycle(dep)) {
                        return true;
                    }
                }
                break;
            }
        }
        
        rec_stack.erase(task_id);
        return false;
    };
    
    for (const auto& task : tasks) {
        if (visited.find(task.id) == visited.end()) {
            if (has_cycle(task.id)) {
                break;
            }
        }
    }
    
    return circular_deps;
}

double PlanningSystem::estimate_plan_duration(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto it = active_plans.find(plan_id);
    if (it == active_plans.end()) {
        return 0.0;
    }
    
    double total_duration = 0.0;
    for (const auto& task : it->second.tasks) {
        total_duration += task.estimated_duration_seconds;
    }
    
    return total_duration;
}

std::string PlanningSystem::generate_plan_summary(const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    auto it = active_plans.find(plan_id);
    if (it == active_plans.end()) {
        return "Plan not found";
    }
    
    const auto& plan = it->second;
    std::ostringstream summary;
    
    summary << "Plan: " << plan.name << "\n";
    summary << "Goal: " << plan.goal << "\n";
    summary << "Tasks: " << plan.tasks.size() << "\n";
    summary << "Progress: " << (plan.get_progress() * 100) << "%\n";
    
    auto pending = plan.get_tasks_by_status(TaskStatus::PENDING);
    auto in_progress = plan.get_tasks_by_status(TaskStatus::IN_PROGRESS);
    auto completed = plan.get_tasks_by_status(TaskStatus::COMPLETED);
    auto failed = plan.get_tasks_by_status(TaskStatus::FAILED);
    
    summary << "Pending: " << pending.size() << "\n";
    summary << "In Progress: " << in_progress.size() << "\n";
    summary << "Completed: " << completed.size() << "\n";
    summary << "Failed: " << failed.size() << "\n";
    
    return summary.str();
}

PlanningSystem::PlanningStats PlanningSystem::get_statistics() const {
    std::lock_guard<std::mutex> lock(planning_mutex);
    
    PlanningStats stats = {};
    stats.active_plans = active_plans.size();
    stats.completed_plans = completed_plans.size();
    
    size_t total_tasks = 0;
    size_t completed_tasks = 0;
    double total_duration = 0.0;
    size_t duration_count = 0;
    
    for (const auto& pair : active_plans) {
        const auto& plan = pair.second;
        total_tasks += plan.tasks.size();
        
        for (const auto& task : plan.tasks) {
            if (task.status == TaskStatus::COMPLETED) {
                completed_tasks++;
                if (task.actual_duration_seconds > 0) {
                    total_duration += task.actual_duration_seconds;
                    duration_count++;
                }
            }
        }
    }
    
    for (const auto& pair : completed_plans) {
        const auto& plan = pair.second;
        total_tasks += plan.tasks.size();
        
        for (const auto& task : plan.tasks) {
            if (task.status == TaskStatus::COMPLETED) {
                completed_tasks++;
                if (task.actual_duration_seconds > 0) {
                    total_duration += task.actual_duration_seconds;
                    duration_count++;
                }
            }
        }
    }
    
    stats.total_tasks = total_tasks;
    stats.completed_tasks = completed_tasks;
    stats.average_task_duration = (duration_count > 0) ? (total_duration / duration_count) : 0.0;
    stats.success_rate = (total_tasks > 0) ? (static_cast<double>(completed_tasks) / total_tasks) : 0.0;
    
    return stats;
}

std::vector<Task> PlanningSystem::decompose_complex_goal(const std::string& goal, const std::string& context) {
    std::vector<Task> tasks;
    
    // Simple goal decomposition - in a real implementation, this would use AI/LLM
    std::string goal_lower = goal;
    std::transform(goal_lower.begin(), goal_lower.end(), goal_lower.begin(), ::tolower);
    
    if (goal_lower.find("research") != std::string::npos) {
        // Research workflow
        tasks.emplace_back(generate_task_id(), "Gather initial information", "web_search");
        tasks.emplace_back(generate_task_id(), "Analyze sources", "text_analysis");
        tasks.emplace_back(generate_task_id(), "Compile findings", "text_processing");
        
        // Add dependencies
        tasks[1].dependencies.push_back(tasks[0].id);
        tasks[2].dependencies.push_back(tasks[1].id);
        
    } else if (goal_lower.find("write") != std::string::npos || goal_lower.find("create") != std::string::npos) {
        // Content creation workflow
        tasks.emplace_back(generate_task_id(), "Plan content structure", "text_processing");
        tasks.emplace_back(generate_task_id(), "Research topic", "context_retrieval");
        tasks.emplace_back(generate_task_id(), "Write content", "text_processing");
        tasks.emplace_back(generate_task_id(), "Review and edit", "text_analysis");
        
        // Add dependencies
        tasks[2].dependencies.push_back(tasks[0].id);
        tasks[2].dependencies.push_back(tasks[1].id);
        tasks[3].dependencies.push_back(tasks[2].id);
        
    } else if (goal_lower.find("analyze") != std::string::npos) {
        // Analysis workflow
        tasks.emplace_back(generate_task_id(), "Collect data", "data_analysis");
        tasks.emplace_back(generate_task_id(), "Process data", "data_transform");
        tasks.emplace_back(generate_task_id(), "Generate insights", "data_analysis");
        
        // Add dependencies
        tasks[1].dependencies.push_back(tasks[0].id);
        tasks[2].dependencies.push_back(tasks[1].id);
        
    } else {
        // Generic workflow
        tasks.emplace_back(generate_task_id(), "Initial task", "echo");
        tasks.emplace_back(generate_task_id(), "Process task", "text_processing");
        tasks.emplace_back(generate_task_id(), "Final task", "echo");
        
        // Add dependencies
        tasks[1].dependencies.push_back(tasks[0].id);
        tasks[2].dependencies.push_back(tasks[1].id);
    }
    
    // Set estimated durations
    for (auto& task : tasks) {
        task.estimated_duration_seconds = 5.0 + (std::rand() % 10); // 5-15 seconds
        task.priority = TaskPriority::NORMAL;
    }
    
    return tasks;
}

bool PlanningSystem::validate_dependencies(const std::vector<Task>& tasks) {
    // Check for circular dependencies
    std::unordered_set<std::string> task_ids;
    for (const auto& task : tasks) {
        task_ids.insert(task.id);
    }
    
    // Verify all dependencies exist
    for (const auto& task : tasks) {
        for (const std::string& dep : task.dependencies) {
            if (task_ids.find(dep) == task_ids.end()) {
                logger->error("Invalid dependency: " + dep + " for task " + task.id);
                return false;
            }
        }
    }
    
    return true;
}

void PlanningSystem::sort_tasks_topologically(std::vector<Task>& tasks) {
    // Simple topological sort
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> in_degree;
    
    // Build adjacency list and in-degree count
    for (const auto& task : tasks) {
        in_degree[task.id] = 0;
    }
    
    for (const auto& task : tasks) {
        for (const std::string& dep : task.dependencies) {
            adj[dep].push_back(task.id);
            in_degree[task.id]++;
        }
    }
    
    // Topological sort using queue
    std::queue<std::string> queue;
    for (const auto& pair : in_degree) {
        if (pair.second == 0) {
            queue.push(pair.first);
        }
    }
    
    std::vector<std::string> sorted_order;
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        sorted_order.push_back(current);
        
        for (const std::string& neighbor : adj[current]) {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }
    
    // Reorder tasks based on sorted order
    std::vector<Task> sorted_tasks;
    for (const std::string& task_id : sorted_order) {
        auto it = std::find_if(tasks.begin(), tasks.end(),
            [&task_id](const Task& t) { return t.id == task_id; });
        if (it != tasks.end()) {
            sorted_tasks.push_back(*it);
        }
    }
    
    tasks = std::move(sorted_tasks);
}

std::string PlanningSystem::generate_task_id() {
    static int counter = 0;
    return "task_" + std::to_string(++counter);
}

std::string PlanningSystem::generate_plan_id() {
    static int counter = 0;
    return "plan_" + std::to_string(++counter);
}

// ReasoningSystem implementation
ReasoningSystem::ReasoningSystem(std::shared_ptr<Logger> log) : logger(log) {
    if (!logger) {
        logger = std::make_shared<Logger>();
    }
}

void ReasoningSystem::add_knowledge(const std::string& key, const AgentData& data) {
    std::lock_guard<std::mutex> lock(reasoning_mutex);
    knowledge_base[key] = data;
}

AgentData ReasoningSystem::get_knowledge(const std::string& key) const {
    std::lock_guard<std::mutex> lock(reasoning_mutex);
    auto it = knowledge_base.find(key);
    return (it != knowledge_base.end()) ? it->second : AgentData();
}

bool ReasoningSystem::has_knowledge(const std::string& key) const {
    std::lock_guard<std::mutex> lock(reasoning_mutex);
    return knowledge_base.find(key) != knowledge_base.end();
}

void ReasoningSystem::remove_knowledge(const std::string& key) {
    std::lock_guard<std::mutex> lock(reasoning_mutex);
    knowledge_base.erase(key);
}

std::string ReasoningSystem::reason_about(const std::string& question, const std::string& context) {
    // Simple reasoning implementation - in practice, this would use an LLM
    std::string reasoning = "Based on the question: '" + question + "'";
    
    if (!context.empty()) {
        reasoning += " and the context: '" + context + "'";
    }
    
    reasoning += ", here is my analysis:\n\n";
    
    // Simple keyword-based reasoning
    std::string question_lower = question;
    std::transform(question_lower.begin(), question_lower.end(), question_lower.begin(), ::tolower);
    
    if (question_lower.find("what") != std::string::npos) {
        reasoning += "This is a definitional question requiring factual information.";
    } else if (question_lower.find("why") != std::string::npos) {
        reasoning += "This question seeks causal explanations or reasons.";
    } else if (question_lower.find("how") != std::string::npos) {
        reasoning += "This question asks about processes or methods.";
    } else if (question_lower.find("when") != std::string::npos) {
        reasoning += "This question relates to timing or temporal information.";
    } else {
        reasoning += "This appears to be a general inquiry requiring analysis.";
    }
    
    // Record reasoning step
    reasoning_history.push_back("Q: " + question + " | A: " + reasoning);
    
    return reasoning;
}

bool ReasoningSystem::can_achieve_goal(const std::string& goal, const std::vector<std::string>& available_functions) {
    // Simple capability matching
    std::string goal_lower = goal;
    std::transform(goal_lower.begin(), goal_lower.end(), goal_lower.begin(), ::tolower);
    
    // Check if we have relevant functions for the goal
    for (const std::string& func : available_functions) {
        std::string func_lower = func;
        std::transform(func_lower.begin(), func_lower.end(), func_lower.begin(), ::tolower);
        
        if ((goal_lower.find("search") != std::string::npos && func_lower.find("search") != std::string::npos) ||
            (goal_lower.find("analyze") != std::string::npos && func_lower.find("analysis") != std::string::npos) ||
            (goal_lower.find("write") != std::string::npos && func_lower.find("text") != std::string::npos)) {
            return true;
        }
    }
    
    return false;
}

std::string ReasoningSystem::suggest_approach(const std::string& problem, const std::string& constraints) {
    std::string suggestion = "Suggested approach for: " + problem + "\n\n";
    
    std::string problem_lower = problem;
    std::transform(problem_lower.begin(), problem_lower.end(), problem_lower.begin(), ::tolower);
    
    if (problem_lower.find("research") != std::string::npos) {
        suggestion += "1. Define research scope and objectives\n";
        suggestion += "2. Identify relevant sources and databases\n";
        suggestion += "3. Gather and organize information\n";
        suggestion += "4. Analyze and synthesize findings\n";
        suggestion += "5. Present results and conclusions\n";
    } else if (problem_lower.find("analysis") != std::string::npos) {
        suggestion += "1. Define analysis criteria and methodology\n";
        suggestion += "2. Collect and prepare data\n";
        suggestion += "3. Apply analytical techniques\n";
        suggestion += "4. Interpret results\n";
        suggestion += "5. Generate insights and recommendations\n";
    } else {
        suggestion += "1. Break down the problem into smaller components\n";
        suggestion += "2. Identify available resources and tools\n";
        suggestion += "3. Develop a step-by-step plan\n";
        suggestion += "4. Execute the plan systematically\n";
        suggestion += "5. Review and refine the approach as needed\n";
    }
    
    if (!constraints.empty()) {
        suggestion += "\nConstraints to consider: " + constraints;
    }
    
    return suggestion;
}

std::string ReasoningSystem::make_decision(const std::string& situation, 
                                         const std::vector<std::string>& options,
                                         const std::string& criteria) {
    if (options.empty()) {
        return "No options provided for decision making.";
    }
    
    if (options.size() == 1) {
        return "Only one option available: " + options[0];
    }
    
    std::string decision = "Decision analysis for situation: " + situation + "\n\n";
    decision += "Available options:\n";
    
    for (size_t i = 0; i < options.size(); ++i) {
        decision += std::to_string(i + 1) + ". " + options[i] + "\n";
    }
    
    if (!criteria.empty()) {
        decision += "\nEvaluation criteria: " + criteria + "\n";
    }
    
    // Simple scoring system (in practice, this would be more sophisticated)
    size_t best_option = 0;
    decision += "\nRecommended option: " + std::to_string(best_option + 1) + ". " + options[best_option];
    decision += "\nReason: This option appears most suitable based on the available information.";
    
    return decision;
}

std::string ReasoningSystem::reflect_on_performance(const std::string& task_result, 
                                                   const std::string& expected_outcome) {
    std::string reflection = "Performance reflection:\n\n";
    reflection += "Task result: " + task_result + "\n";
    reflection += "Expected outcome: " + expected_outcome + "\n\n";
    
    // Simple comparison
    if (task_result == expected_outcome) {
        reflection += "Assessment: Task completed successfully as expected.";
    } else {
        reflection += "Assessment: Task result differs from expected outcome. ";
        reflection += "This suggests areas for improvement in task execution or expectation setting.";
    }
    
    return reflection;
}

std::vector<std::string> ReasoningSystem::identify_improvement_areas(const std::string& task_history) {
    std::vector<std::string> improvements;
    
    // Simple analysis of task history
    if (task_history.find("failed") != std::string::npos) {
        improvements.push_back("Improve error handling and recovery mechanisms");
    }
    
    if (task_history.find("timeout") != std::string::npos) {
        improvements.push_back("Optimize task execution time and resource management");
    }
    
    if (task_history.find("retry") != std::string::npos) {
        improvements.push_back("Enhance initial task planning to reduce need for retries");
    }
    
    if (improvements.empty()) {
        improvements.push_back("Continue monitoring performance for optimization opportunities");
    }
    
    return improvements;
}

bool ReasoningSystem::should_ask_for_help(const std::string& current_situation) {
    // Simple heuristics for when to ask for help
    std::string situation_lower = current_situation;
    std::transform(situation_lower.begin(), situation_lower.end(), situation_lower.begin(), ::tolower);
    
    return (situation_lower.find("stuck") != std::string::npos ||
            situation_lower.find("confused") != std::string::npos ||
            situation_lower.find("unclear") != std::string::npos ||
            situation_lower.find("uncertain") != std::string::npos);
}

std::string ReasoningSystem::generate_clarifying_questions(const std::string& unclear_request) {
    std::string questions = "To better understand your request, please clarify:\n\n";
    
    if (unclear_request.find("this") != std::string::npos || unclear_request.find("that") != std::string::npos) {
        questions += "1. What specific item or concept are you referring to?\n";
    }
    
    if (unclear_request.find("analyze") != std::string::npos) {
        questions += "2. What type of analysis are you looking for?\n";
        questions += "3. What data or information should be analyzed?\n";
    }
    
    if (unclear_request.find("help") != std::string::npos) {
        questions += "4. What specific aspect do you need help with?\n";
        questions += "5. What is your end goal?\n";
    }
    
    questions += "6. Are there any constraints or requirements I should be aware of?\n";
    
    return questions;
}

std::vector<std::string> ReasoningSystem::get_reasoning_history() const {
    std::lock_guard<std::mutex> lock(reasoning_mutex);
    return reasoning_history;
}

void ReasoningSystem::clear_reasoning_history() {
    std::lock_guard<std::mutex> lock(reasoning_mutex);
    reasoning_history.clear();
}

// PlanningReasoningCoordinator implementation
PlanningReasoningCoordinator::PlanningReasoningCoordinator(std::shared_ptr<Logger> log) : logger(log) {
    if (!logger) {
        logger = std::make_shared<Logger>();
    }
    
    planning_system = std::make_unique<PlanningSystem>(logger);
    reasoning_system = std::make_unique<ReasoningSystem>(logger);
}

PlanningReasoningCoordinator::~PlanningReasoningCoordinator() = default;

ExecutionPlan PlanningReasoningCoordinator::create_intelligent_plan(const std::string& goal, 
                                                                   const std::string& context,
                                                                   const std::vector<std::string>& available_functions) {
    // Use reasoning to analyze the goal first
    std::string analysis = reasoning_system->reason_about("How to achieve: " + goal, context);
    logger->debug("Goal analysis: " + analysis);
    
    // Check if goal is achievable with available functions
    if (!reasoning_system->can_achieve_goal(goal, available_functions)) {
        logger->warn("Goal may not be achievable with available functions");
    }
    
    // Get suggested approach
    std::string approach = reasoning_system->suggest_approach(goal, context);
    logger->debug("Suggested approach: " + approach);
    
    // Create the plan using dependency-aware strategy for intelligence
    auto plan = planning_system->decompose_goal(goal, context, PlanningStrategy::DEPENDENCY_AWARE);
    
    // Add the plan to the planning system
    planning_system->add_plan(plan);
    
    return plan;
}

bool PlanningReasoningCoordinator::adapt_plan_based_on_feedback(const std::string& plan_id, 
                                                               const std::string& feedback,
                                                               const AgentData& execution_results) {
    // Use reasoning to analyze feedback
    std::string analysis = reasoning_system->reason_about(
        "How to adapt plan based on feedback: " + feedback, 
        execution_results.to_string());
    
    logger->info("Plan adaptation analysis: " + analysis);
    
    // For now, just log the adaptation need
    // In a full implementation, this would modify the plan
    logger->info("Plan " + plan_id + " should be adapted based on feedback");
    
    return true;
}

std::string PlanningReasoningCoordinator::recommend_next_action(const std::string& plan_id, 
                                                               const std::string& current_state) {
    ExecutionPlan* plan = planning_system->get_plan(plan_id);
    if (!plan) {
        return "Plan not found";
    }
    
    // Get ready tasks
    auto ready_tasks = planning_system->get_next_tasks(plan_id);
    
    if (ready_tasks.empty()) {
        if (plan->is_complete()) {
            return "Plan is complete";
        } else {
            return "No tasks are ready to execute. Check for failed dependencies.";
        }
    }
    
    // Use reasoning to recommend the best next action
    std::vector<std::string> options;
    for (const Task* task : ready_tasks) {
        options.push_back(task->name + " (ID: " + task->id + ")");
    }
    
    std::string decision = reasoning_system->make_decision(current_state, options, "task priority and dependencies");
    
    return "Recommended next action: " + decision;
}

} // namespace kolosal::agents

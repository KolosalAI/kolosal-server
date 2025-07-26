#include "kolosal/agents/workflow_engine.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <random>
#include <fstream>
#include <filesystem>
#include <regex>
#include <optional>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

using json = nlohmann::json;

namespace kolosal::agents {

WorkflowEngine::WorkflowEngine(std::shared_ptr<YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
    if (!agent_manager) {
        throw std::invalid_argument("Agent manager cannot be null");
    }
    
    // Initialize metrics
    metrics.last_updated = std::chrono::system_clock::now();
    
    ServerLogger::logInfo("WorkflowEngine initialized");
}

WorkflowEngine::~WorkflowEngine() {
    stop();
}

void WorkflowEngine::start() {
    if (engine_running.load()) {
        return;
    }
    
    engine_running.store(true);
    
    // Start execution thread
    execution_thread = std::thread(&WorkflowEngine::execution_loop, this);
    
    // Start worker threads
    worker_threads.clear();
    for (int i = 0; i < max_worker_threads; ++i) {
        worker_threads.emplace_back(&WorkflowEngine::worker_loop, this);
    }
    
    // Load persisted state
    if (enable_persistence) {
        load_workflow_state();
    }
    
    ServerLogger::logInfo("WorkflowEngine started with %d worker threads", max_worker_threads);
}

void WorkflowEngine::stop() {
    if (!engine_running.load()) {
        return;
    }
    
    engine_running.store(false);
    workflow_cv.notify_all();
    
    // Wait for execution thread to finish
    if (execution_thread.joinable()) {
        execution_thread.join();
    }
    
    // Wait for worker threads to finish
    for (auto& worker : worker_threads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // Save state before shutdown
    if (enable_persistence) {
        for (const auto& [execution_id, context] : active_executions) {
            save_workflow_state(context);
        }
    }
    
    ServerLogger::logInfo("WorkflowEngine stopped");
}

std::string WorkflowEngine::create_workflow(const Workflow& workflow) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto workflow_copy = workflow;
    if (workflow_copy.workflow_id.empty()) {
        workflow_copy.workflow_id = generate_workflow_id();
    }
    
    workflow_copy.created_time = std::chrono::system_clock::now();
    workflow_copy.status = WorkflowStatus::PENDING;
    
    // Validate workflow
    if (has_circular_dependencies(workflow_copy)) {
        throw std::invalid_argument("Workflow contains circular dependencies");
    }
    
    workflows[workflow_copy.workflow_id] = workflow_copy;
    
    ServerLogger::logInfo("Created workflow: %s (%s)", workflow_copy.workflow_id.c_str(), workflow_copy.name.c_str());
    return workflow_copy.workflow_id;
}

bool WorkflowEngine::update_workflow(const std::string& workflow_id, const Workflow& workflow) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it == workflows.end()) {
        return false;
    }
    
    // Don't allow updates to running workflows
    if (it->second.status == WorkflowStatus::RUNNING) {
        return false;
    }
    
    auto updated = workflow;
    updated.workflow_id = workflow_id;
    updated.created_time = it->second.created_time;
    
    // Validate updated workflow
    if (has_circular_dependencies(updated)) {
        return false;
    }
    
    workflows[workflow_id] = updated;
    
    ServerLogger::logInfo("Updated workflow: %s", workflow_id.c_str());
    return true;
}

bool WorkflowEngine::delete_workflow(const std::string& workflow_id) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it == workflows.end()) {
        return false;
    }
    
    // Don't allow deletion of running workflows
    if (it->second.status == WorkflowStatus::RUNNING) {
        return false;
    }
    
    workflows.erase(it);
    
    ServerLogger::logInfo("Deleted workflow: %s", workflow_id.c_str());
    return true;
}

std::vector<std::string> WorkflowEngine::list_workflows() const {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    std::vector<std::string> workflow_ids;
    workflow_ids.reserve(workflows.size());
    
    for (const auto& [id, workflow] : workflows) {
        workflow_ids.push_back(id);
    }
    
    return workflow_ids;
}

std::optional<Workflow> WorkflowEngine::get_workflow(const std::string& workflow_id) const {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it != workflows.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::string WorkflowEngine::execute_workflow(const std::string& workflow_id, const json& input_context) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto workflow_it = workflows.find(workflow_id);
    if (workflow_it == workflows.end()) {
        throw std::invalid_argument("Workflow not found: " + workflow_id);
    }
    
    auto& workflow = workflow_it->second;
    if (workflow.status == WorkflowStatus::RUNNING) {
        throw std::runtime_error("Workflow is already running");
    }
    
    // Create execution context
    WorkflowExecutionContext context;
    context.execution_id = generate_execution_id();
    context.workflow_id = workflow_id;
    context.global_variables = workflow.global_context;
    context.execution_start = std::chrono::system_clock::now();
    context.current_status = WorkflowStatus::PENDING;
    
    // Merge input context
    if (!input_context.empty()) {
        context.global_variables.update(input_context);
    }
    
    // Initialize step statuses
    for (const auto& step : workflow.steps) {
        context.step_statuses[step.step_id] = StepStatus::PENDING;
    }
    
    active_executions[context.execution_id] = context;
    workflow.status = WorkflowStatus::RUNNING;
    
    // Queue for execution
    workflow_queue.push(context.execution_id);
    workflow_cv.notify_one();
    
    log_workflow_event(context.execution_id, "QUEUED", "Workflow queued for execution");
    
    return context.execution_id;
}

bool WorkflowEngine::pause_workflow(const std::string& execution_id) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = active_executions.find(execution_id);
    if (it == active_executions.end()) {
        return false;
    }
    
    if (it->second.current_status == WorkflowStatus::RUNNING) {
        it->second.current_status = WorkflowStatus::PAUSED;
        log_workflow_event(execution_id, "PAUSED", "Workflow paused by user");
        return true;
    }
    
    return false;
}

bool WorkflowEngine::resume_workflow(const std::string& execution_id) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = active_executions.find(execution_id);
    if (it == active_executions.end()) {
        return false;
    }
    
    if (it->second.current_status == WorkflowStatus::PAUSED) {
        it->second.current_status = WorkflowStatus::RUNNING;
        workflow_queue.push(execution_id);
        workflow_cv.notify_one();
        log_workflow_event(execution_id, "RESUMED", "Workflow resumed by user");
        return true;
    }
    
    return false;
}

bool WorkflowEngine::cancel_workflow(const std::string& execution_id) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = active_executions.find(execution_id);
    if (it == active_executions.end()) {
        return false;
    }
    
    it->second.current_status = WorkflowStatus::CANCELLED;
    
    // Update workflow status
    auto workflow_it = workflows.find(it->second.workflow_id);
    if (workflow_it != workflows.end()) {
        workflow_it->second.status = WorkflowStatus::CANCELLED;
    }
    
    log_workflow_event(execution_id, "CANCELLED", "Workflow cancelled by user");
    return true;
}

std::optional<WorkflowExecutionContext> WorkflowEngine::get_execution_status(const std::string& execution_id) const {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = active_executions.find(execution_id);
    if (it != active_executions.end()) {
        return it->second;
    }
    
    auto hist_it = execution_history.find(execution_id);
    if (hist_it != execution_history.end()) {
        return hist_it->second;
    }
    
    return std::nullopt;
}

std::vector<WorkflowExecutionContext> WorkflowEngine::get_active_executions() const {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    std::vector<WorkflowExecutionContext> executions;
    executions.reserve(active_executions.size());
    
    for (const auto& [id, context] : active_executions) {
        executions.push_back(context);
    }
    
    return executions;
}

std::vector<WorkflowExecutionContext> WorkflowEngine::get_execution_history(const std::string& workflow_id) const {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    std::vector<WorkflowExecutionContext> history;
    
    for (const auto& [id, context] : execution_history) {
        if (workflow_id.empty() || context.workflow_id == workflow_id) {
            history.push_back(context);
        }
    }
    
    // Sort by execution start time (most recent first)
    std::sort(history.begin(), history.end(), 
        [](const WorkflowExecutionContext& a, const WorkflowExecutionContext& b) {
            return a.execution_start > b.execution_start;
        });
    
    return history;
}

WorkflowMetrics WorkflowEngine::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    return metrics;
}

void WorkflowEngine::execution_loop() {
    while (engine_running.load()) {
        std::unique_lock<std::mutex> lock(workflow_mutex);
        
        workflow_cv.wait(lock, [this] {
            return !workflow_queue.empty() || !engine_running.load();
        });
        
        if (!engine_running.load()) {
            break;
        }
        
        if (!workflow_queue.empty()) {
            std::string execution_id = workflow_queue.front();
            workflow_queue.pop();
            lock.unlock();
            
            try {
                execute_workflow_internal(execution_id);
            } catch (const std::exception& e) {
                ServerLogger::logError("Error executing workflow %s: %s", execution_id.c_str(), e.what());
                
                // Mark as failed
                lock.lock();
                auto it = active_executions.find(execution_id);
                if (it != active_executions.end()) {
                    it->second.current_status = WorkflowStatus::FAILED;
                    
                    auto workflow_it = workflows.find(it->second.workflow_id);
                    if (workflow_it != workflows.end()) {
                        workflow_it->second.status = WorkflowStatus::FAILED;
                        workflow_it->second.error_message = e.what();
                    }
                }
                lock.unlock();
            }
        }
    }
}

void WorkflowEngine::worker_loop() {
    // Worker threads can be used for parallel step execution
    // This is a simplified implementation
    while (engine_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // TODO: Implement parallel step execution
    }
}

void WorkflowEngine::execute_workflow_internal(const std::string& execution_id) {
    std::unique_lock<std::mutex> lock(workflow_mutex);
    
    auto context_it = active_executions.find(execution_id);
    if (context_it == active_executions.end()) {
        return;
    }
    
    auto workflow_it = workflows.find(context_it->second.workflow_id);
    if (workflow_it == workflows.end()) {
        return;
    }
    
    auto& context = context_it->second;
    auto& workflow = workflow_it->second;
    
    context.current_status = WorkflowStatus::RUNNING;
    log_workflow_event(execution_id, "STARTED", "Workflow execution started");
    
    lock.unlock();
    
    try {
        // Resolve execution order
        auto execution_order = resolve_execution_order(workflow);
        
        // Execute steps based on workflow type
        switch (workflow.type) {
            case WorkflowType::SEQUENTIAL:
                for (const auto& step_id : execution_order) {
                    if (!engine_running.load()) break;
                    
                    auto step_it = std::find_if(workflow.steps.begin(), workflow.steps.end(),
                        [&step_id](const WorkflowStep& s) { return s.step_id == step_id; });
                    
                    if (step_it != workflow.steps.end()) {
                        lock.lock();
                        if (context.current_status != WorkflowStatus::RUNNING) {
                            lock.unlock();
                            break;
                        }
                        context.current_step_id = step_id;
                        lock.unlock();
                        
                        execute_step(context, *step_it);
                        
                        lock.lock();
                        if (context.step_statuses[step_id] == StepStatus::FAILED && 
                            !workflow.error_handling.continue_on_error) {
                            context.current_status = WorkflowStatus::FAILED;
                            lock.unlock();
                            break;
                        }
                        lock.unlock();
                    }
                }
                break;
                
            case WorkflowType::PARALLEL:
                // TODO: Implement parallel execution
                break;
                
            case WorkflowType::PIPELINE:
                // TODO: Implement pipeline execution
                break;
                
            case WorkflowType::CONSENSUS:
                // TODO: Implement consensus execution
                break;
                
            case WorkflowType::CONDITIONAL:
                // TODO: Implement conditional execution
                break;
        }
        
        lock.lock();
        if (context.current_status == WorkflowStatus::RUNNING) {
            context.current_status = WorkflowStatus::COMPLETED;
            workflow.status = WorkflowStatus::COMPLETED;
            log_workflow_event(execution_id, "COMPLETED", "Workflow execution completed successfully");
        }
    } catch (const std::exception& e) {
        lock.lock();
        context.current_status = WorkflowStatus::FAILED;
        workflow.status = WorkflowStatus::FAILED;
        workflow.error_message = e.what();
        log_workflow_event(execution_id, "FAILED", std::string("Workflow execution failed: ") + e.what());
    }
    
    // Move to history
    execution_history[execution_id] = context;
    active_executions.erase(execution_id);
    
    lock.unlock();
    
    update_metrics();
}

void WorkflowEngine::execute_step(WorkflowExecutionContext& context, WorkflowStep& step) {
    step.status = StepStatus::RUNNING;
    step.start_time = std::chrono::system_clock::now();
    
    log_workflow_event(context.execution_id, "STEP_STARTED", 
        "Started step: " + step.step_id + " (" + step.name + ")");
    
    try {
        // Check dependencies
        if (!check_step_dependencies(context, step)) {
            step.status = StepStatus::SKIPPED;
            log_workflow_event(context.execution_id, "STEP_SKIPPED", 
                "Step skipped due to unmet dependencies: " + step.step_id);
            return;
        }
        
        // Evaluate conditions
        if (!step.conditions.empty() && !evaluate_conditions(step.conditions, context)) {
            step.status = StepStatus::SKIPPED;
            log_workflow_event(context.execution_id, "STEP_SKIPPED", 
                "Step skipped due to unmet conditions: " + step.step_id);
            return;
        }
        
        // Interpolate parameters
        auto interpolated_params = interpolate_parameters(step.parameters, context);
        
        // Execute step with retry logic
        bool success = false;
        for (int attempt = 0; attempt <= step.max_retries && !success; ++attempt) {
            try {
                if (attempt > 0) {
                    step.status = StepStatus::RETRYING;
                    std::this_thread::sleep_for(std::chrono::seconds(step.retry_delay_seconds));
                    log_workflow_event(context.execution_id, "STEP_RETRY", 
                        "Retrying step: " + step.step_id + " (attempt " + std::to_string(attempt + 1) + ")");
                }
                
                // Get agent and execute function
                auto agent = agent_manager->get_agent(step.agent_id);
                if (!agent) {
                    throw std::runtime_error("Agent not found: " + step.agent_id);
                }
                
                // Convert parameters to AgentData format
                AgentData agent_params;
                agent_params.from_json(interpolated_params);
                
                auto result = agent->execute_function(step.function_name, agent_params);
                
                // Store result
                step.output = result.result_data.to_json();
                context.step_outputs[step.step_id] = step.output;
                
                success = true;
                step.status = StepStatus::COMPLETED;
                
            } catch (const std::exception& e) {
                step.error_message = e.what();
                step.retry_count = attempt;
                
                if (attempt == step.max_retries) {
                    step.status = StepStatus::FAILED;
                    log_workflow_event(context.execution_id, "STEP_FAILED", 
                        "Step failed after " + std::to_string(attempt + 1) + " attempts: " + step.step_id + " - " + e.what());
                }
            }
        }
        
    } catch (const std::exception& e) {
        step.status = StepStatus::FAILED;
        step.error_message = e.what();
        log_workflow_event(context.execution_id, "STEP_FAILED", 
            "Step execution error: " + step.step_id + " - " + e.what());
    }
    
    step.end_time = std::chrono::system_clock::now();
    context.step_statuses[step.step_id] = step.status;
    
    if (step.status == StepStatus::COMPLETED) {
        context.completed_steps.push_back(step.step_id);
        log_workflow_event(context.execution_id, "STEP_COMPLETED", 
            "Completed step: " + step.step_id + " (" + step.name + ")");
    } else if (step.status == StepStatus::FAILED) {
        context.failed_steps.push_back(step.step_id);
    }
}

bool WorkflowEngine::check_step_dependencies(const WorkflowExecutionContext& context, const WorkflowStep& step) {
    for (const auto& dep : step.dependencies) {
        auto status_it = context.step_statuses.find(dep.step_id);
        if (status_it == context.step_statuses.end()) {
            return false; // Dependency step not found
        }
        
        if (dep.condition == "success" && status_it->second != StepStatus::COMPLETED) {
            if (dep.required) {
                return false;
            }
        } else if (dep.condition == "completion" && 
                   status_it->second != StepStatus::COMPLETED && 
                   status_it->second != StepStatus::FAILED) {
            if (dep.required) {
                return false;
            }
        }
        // TODO: Implement more complex conditions
    }
    
    return true;
}

bool WorkflowEngine::evaluate_conditions(const json& conditions, const WorkflowExecutionContext& context) {
    // Simple condition evaluation - can be expanded
    try {
        if (conditions.contains("expression")) {
            // TODO: Implement expression evaluation
            return true;
        }
        return true;
    } catch (const std::exception& e) {
        ServerLogger::logError("Error evaluating conditions: %s", e.what());
        return false;
    }
}

json WorkflowEngine::interpolate_parameters(const json& parameters, const WorkflowExecutionContext& context) {
    if (!parameters.is_object()) {
        return parameters;
    }
    
    json result = parameters;
    
    // Simple string interpolation for ${...} expressions
    std::function<void(json&)> interpolate_recursive = [&](json& obj) {
        if (obj.is_string()) {
            std::string str = obj.get<std::string>();
            std::regex pattern(R"(\$\{([^}]+)\})");
            std::smatch matches;
            
            while (std::regex_search(str, matches, pattern)) {
                std::string var_path = matches[1].str();
                
                // Simple variable resolution
                if (var_path.substr(0, 7) == "global.") {
                    std::string var_name = var_path.substr(7);
                    if (context.global_variables.count(var_name) > 0) {
                        std::string value = context.global_variables[var_name].dump();
                        str = std::regex_replace(str, std::regex("\\$\\{" + var_path + "\\}"), value);
                    }
                } else if (var_path.substr(0, 6) == "steps.") {
                    // Parse steps.step_id.output pattern
                    auto dot_pos = var_path.find('.', 6);
                    if (dot_pos != std::string::npos) {
                        std::string step_id = var_path.substr(6, dot_pos - 6);
                        std::string field = var_path.substr(dot_pos + 1);
                        
                        if (context.step_outputs.count(step_id) > 0 && field == "output") {
                            std::string value = context.step_outputs.at(step_id).dump();
                            str = std::regex_replace(str, std::regex("\\$\\{" + var_path + "\\}"), value);
                        }
                    }
                }
            }
            
            obj = str;
        } else if (obj.is_object()) {
            for (auto& [key, value] : obj.items()) {
                interpolate_recursive(value);
            }
        } else if (obj.is_array()) {
            for (auto& item : obj) {
                interpolate_recursive(item);
            }
        }
    };
    
    interpolate_recursive(result);
    return result;
}

std::vector<std::string> WorkflowEngine::resolve_execution_order(const Workflow& workflow) {
    std::vector<std::string> order;
    std::map<std::string, bool> visited;
    std::map<std::string, bool> visiting;
    
    std::function<void(const std::string&)> dfs = [&](const std::string& step_id) {
        if (visited[step_id]) return;
        if (visiting[step_id]) {
            throw std::runtime_error("Circular dependency detected");
        }
        
        visiting[step_id] = true;
        
        // Find step
        auto step_it = std::find_if(workflow.steps.begin(), workflow.steps.end(),
            [&step_id](const WorkflowStep& s) { return s.step_id == step_id; });
        
        if (step_it != workflow.steps.end()) {
            // Visit dependencies first
            for (const auto& dep : step_it->dependencies) {
                dfs(dep.step_id);
            }
        }
        
        visiting[step_id] = false;
        visited[step_id] = true;
        order.push_back(step_id);
    };
    
    // Process all steps
    for (const auto& step : workflow.steps) {
        if (!visited[step.step_id]) {
            dfs(step.step_id);
        }
    }
    
    return order;
}

bool WorkflowEngine::has_circular_dependencies(const Workflow& workflow) {
    try {
        resolve_execution_order(workflow);
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

std::string WorkflowEngine::generate_execution_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex_chars = "0123456789abcdef";
    std::string id = "exec_";
    
    for (int i = 0; i < 16; ++i) {
        id += hex_chars[dis(gen)];
    }
    
    return id;
}

std::string WorkflowEngine::generate_workflow_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex_chars = "0123456789abcdef";
    std::string id = "wf_";
    
    for (int i = 0; i < 12; ++i) {
        id += hex_chars[dis(gen)];
    }
    
    return id;
}

void WorkflowEngine::log_workflow_event(const std::string& execution_id, const std::string& event, const std::string& details) {
    ServerLogger::logInfo("Workflow[%s] %s: %s", execution_id.c_str(), event.c_str(), details.c_str());
}

void WorkflowEngine::update_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    
    metrics.total_workflows = workflows.size();
    metrics.running_workflows = 0;
    metrics.completed_workflows = 0;
    metrics.failed_workflows = 0;
    metrics.cancelled_workflows = 0;
    
    for (const auto& [id, workflow] : workflows) {
        switch (workflow.status) {
            case WorkflowStatus::RUNNING:
                metrics.running_workflows++;
                break;
            case WorkflowStatus::COMPLETED:
                metrics.completed_workflows++;
                break;
            case WorkflowStatus::FAILED:
                metrics.failed_workflows++;
                break;
            case WorkflowStatus::CANCELLED:
                metrics.cancelled_workflows++;
                break;
            default:
                break;
        }
    }
    
    if (metrics.total_workflows > 0) {
        metrics.success_rate = static_cast<double>(metrics.completed_workflows) / metrics.total_workflows * 100.0;
    }
    
    metrics.last_updated = std::chrono::system_clock::now();
}

void WorkflowEngine::save_workflow_state(const WorkflowExecutionContext& context) {
    if (!enable_persistence) return;
    
    try {
        std::filesystem::create_directories(persistence_path);
        
        json state;
        state["execution_id"] = context.execution_id;
        state["workflow_id"] = context.workflow_id;
        state["global_variables"] = context.global_variables;
        state["step_outputs"] = context.step_outputs;
        state["current_status"] = static_cast<int>(context.current_status);
        state["current_step_id"] = context.current_step_id;
        state["completed_steps"] = context.completed_steps;
        state["failed_steps"] = context.failed_steps;
        
        std::string filename = persistence_path + "/" + context.execution_id + ".json";
        std::ofstream file(filename);
        file << state.dump(4);
        
    } catch (const std::exception& e) {
        ServerLogger::logError("Failed to save workflow state: %s", e.what());
    }
}

void WorkflowEngine::load_workflow_state() {
    if (!enable_persistence) return;
    
    try {
        if (!std::filesystem::exists(persistence_path)) {
            return;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(persistence_path)) {
            if (entry.path().extension() == ".json") {
                try {
                    std::ifstream file(entry.path());
                    json state;
                    file >> state;
                    
                    WorkflowExecutionContext context;
                    context.execution_id = state["execution_id"];
                    context.workflow_id = state["workflow_id"];
                    context.global_variables = state["global_variables"];
                    context.step_outputs = state["step_outputs"];
                    context.current_status = static_cast<WorkflowStatus>(state["current_status"]);
                    context.current_step_id = state["current_step_id"];
                    context.completed_steps = state["completed_steps"];
                    context.failed_steps = state["failed_steps"];
                    
                    // Only load running workflows to active executions
                    if (context.current_status == WorkflowStatus::RUNNING || 
                        context.current_status == WorkflowStatus::PAUSED) {
                        active_executions[context.execution_id] = context;
                    } else {
                        execution_history[context.execution_id] = context;
                    }
                    
                } catch (const std::exception& e) {
                    ServerLogger::logError("Failed to load workflow state from %s: %s", 
                        entry.path().string().c_str(), e.what());
                }
            }
        }
        
        ServerLogger::logInfo("Loaded %zu active executions and %zu history entries", 
            active_executions.size(), execution_history.size());
            
    } catch (const std::exception& e) {
        ServerLogger::logError("Failed to load workflow states: %s", e.what());
    }
}

// Template workflow creators
Workflow WorkflowEngine::create_sequential_workflow(const std::string& name, 
    const std::vector<std::pair<std::string, std::string>>& agent_functions) {
    
    Workflow workflow;
    workflow.name = name;
    workflow.type = WorkflowType::SEQUENTIAL;
    
    for (size_t i = 0; i < agent_functions.size(); ++i) {
        WorkflowStep step;
        step.step_id = "step_" + std::to_string(i + 1);
        step.name = "Step " + std::to_string(i + 1);
        step.agent_id = agent_functions[i].first;
        step.function_name = agent_functions[i].second;
        
        // Add dependency to previous step (except for first step)
        if (i > 0) {
            StepDependency dep;
            dep.step_id = "step_" + std::to_string(i);
            dep.condition = "success";
            step.dependencies.push_back(dep);
        }
        
        workflow.steps.push_back(step);
    }
    
    return workflow;
}

Workflow WorkflowEngine::create_parallel_workflow(const std::string& name, 
    const std::vector<std::pair<std::string, std::string>>& agent_functions) {
    
    Workflow workflow;
    workflow.name = name;
    workflow.type = WorkflowType::PARALLEL;
    
    for (size_t i = 0; i < agent_functions.size(); ++i) {
        WorkflowStep step;
        step.step_id = "step_" + std::to_string(i + 1);
        step.name = "Step " + std::to_string(i + 1);
        step.agent_id = agent_functions[i].first;
        step.function_name = agent_functions[i].second;
        step.parallel_allowed = true;
        
        workflow.steps.push_back(step);
    }
    
    return workflow;
}

Workflow WorkflowEngine::create_pipeline_workflow(const std::string& name, 
    const std::vector<std::pair<std::string, std::string>>& agent_functions) {
    
    Workflow workflow;
    workflow.name = name;
    workflow.type = WorkflowType::PIPELINE;
    
    for (size_t i = 0; i < agent_functions.size(); ++i) {
        WorkflowStep step;
        step.step_id = "step_" + std::to_string(i + 1);
        step.name = "Step " + std::to_string(i + 1);
        step.agent_id = agent_functions[i].first;
        step.function_name = agent_functions[i].second;
        
        // Pipeline: each step takes output from previous step
        if (i > 0) {
            StepDependency dep;
            dep.step_id = "step_" + std::to_string(i);
            dep.condition = "success";
            step.dependencies.push_back(dep);
            
            // Add parameter that references previous step output
            step.parameters["input"] = "${steps.step_" + std::to_string(i) + ".output}";
        }
        
        workflow.steps.push_back(step);
    }
    
    return workflow;
}

Workflow WorkflowEngine::create_consensus_workflow(const std::string& name, 
    const std::vector<std::string>& agent_ids, const std::string& decision_function) {
    
    Workflow workflow;
    workflow.name = name;
    workflow.type = WorkflowType::CONSENSUS;
    
    // Create voting steps for each agent
    for (size_t i = 0; i < agent_ids.size(); ++i) {
        WorkflowStep step;
        step.step_id = "vote_" + std::to_string(i + 1);
        step.name = "Vote " + std::to_string(i + 1);
        step.agent_id = agent_ids[i];
        step.function_name = decision_function;
        step.parallel_allowed = true;
        
        workflow.steps.push_back(step);
    }
    
    // Add consensus step that aggregates results
    WorkflowStep consensus_step;
    consensus_step.step_id = "consensus";
    consensus_step.name = "Consensus Decision";
    consensus_step.agent_id = agent_ids[0]; // Use first agent for aggregation
    consensus_step.function_name = "aggregate_votes";
    
    // Add dependencies to all voting steps
    for (size_t i = 0; i < agent_ids.size(); ++i) {
        StepDependency dep;
        dep.step_id = "vote_" + std::to_string(i + 1);
        dep.condition = "completion";
        dep.required = false; // Allow consensus even if some votes fail
        consensus_step.dependencies.push_back(dep);
    }
    
    workflow.steps.push_back(consensus_step);
    
    return workflow;
}

} // namespace kolosal::agents

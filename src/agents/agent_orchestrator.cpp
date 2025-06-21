#include "kolosal/agents/agent_orchestrator.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <random>

namespace kolosal::agents {

AgentOrchestrator::AgentOrchestrator(std::shared_ptr<YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
    // Initialize logger - assuming logger is available through agent_manager or create new one
    // logger = agent_manager->get_logger(); // If available
}

AgentOrchestrator::~AgentOrchestrator() {
    stop();
}

void AgentOrchestrator::start() {
    if (orchestrator_running.load()) {
        return;
    }
    
    orchestrator_running.store(true);
    orchestrator_thread = std::thread(&AgentOrchestrator::orchestrator_worker, this);
    
    ServerLogger::logInfo("Agent orchestrator started");
}

void AgentOrchestrator::stop() {
    if (!orchestrator_running.load()) {
        return;
    }
    
    orchestrator_running.store(false);
    workflow_cv.notify_all();
    
    if (orchestrator_thread.joinable()) {
        orchestrator_thread.join();
    }
    
    ServerLogger::logInfo("Agent orchestrator stopped");
}

bool AgentOrchestrator::register_workflow(const Workflow& workflow) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    if (workflows.find(workflow.workflow_id) != workflows.end()) {
        ServerLogger::logWarning("Workflow %s already exists", workflow.workflow_id.c_str());
        return false;
    }
    
    workflows[workflow.workflow_id] = workflow;
    ServerLogger::logInfo("Registered workflow: %s", workflow.workflow_id.c_str());
    return true;
}

bool AgentOrchestrator::execute_workflow(const std::string& workflow_id, const AgentData& input_context) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it == workflows.end()) {
        ServerLogger::logError("Workflow %s not found", workflow_id.c_str());
        return false;
    }
    
    // Execute synchronously
    WorkflowResult result = execute_workflow_internal(it->second, input_context);
    workflow_results[workflow_id] = std::move(result);
    
    return workflow_results[workflow_id].success;
}

bool AgentOrchestrator::execute_workflow_async(const std::string& workflow_id, const AgentData& input_context) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it == workflows.end()) {
        ServerLogger::logError("Workflow %s not found", workflow_id.c_str());
        return false;
    }
    
    // Add to queue for async execution
    workflow_queue.push(workflow_id);
    workflow_cv.notify_one();
    
    return true;
}

AgentOrchestrator::WorkflowResult AgentOrchestrator::get_workflow_result(const std::string& workflow_id) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflow_results.find(workflow_id);
    if (it != workflow_results.end()) {
        return it->second;
    }
    
    // Return empty result if not found
    WorkflowResult empty_result;
    empty_result.workflow_id = workflow_id;
    empty_result.error_message = "Workflow result not found";
    return empty_result;
}

bool AgentOrchestrator::cancel_workflow(const std::string& workflow_id) {
    // TODO: Implement workflow cancellation
    ServerLogger::logInfo("Cancelling workflow: %s", workflow_id.c_str());
    return true;
}

std::vector<std::string> AgentOrchestrator::list_workflows() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(workflow_mutex));
    
    std::vector<std::string> workflow_ids;
    for (const auto& [id, workflow] : workflows) {
        workflow_ids.push_back(id);
    }
    return workflow_ids;
}

bool AgentOrchestrator::remove_workflow(const std::string& workflow_id) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it == workflows.end()) {
        return false;
    }
    
    workflows.erase(it);
    workflow_results.erase(workflow_id);
    
    ServerLogger::logInfo("Removed workflow: %s", workflow_id.c_str());
    return true;
}

bool AgentOrchestrator::create_collaboration_group(const CollaborationGroup& group) {
    std::lock_guard<std::mutex> lock(collaboration_mutex);
    
    if (collaboration_groups.find(group.group_id) != collaboration_groups.end()) {
        ServerLogger::logWarning("Collaboration group %s already exists", group.group_id.c_str());
        return false;
    }
    
    collaboration_groups[group.group_id] = group;
    ServerLogger::logInfo("Created collaboration group: %s", group.group_id.c_str());
    return true;
}

bool AgentOrchestrator::execute_collaboration(const std::string& group_id, const std::string& task_description, const AgentData& input_data) {
    std::lock_guard<std::mutex> lock(collaboration_mutex);
    
    auto it = collaboration_groups.find(group_id);
    if (it == collaboration_groups.end()) {
        ServerLogger::logError("Collaboration group %s not found", group_id.c_str());
        return false;
    }
    
    const CollaborationGroup& group = it->second;
    AgentData result;
    
    switch (group.pattern) {
    case CollaborationPattern::SEQUENTIAL:
        result = execute_sequential_collaboration(group, input_data);
        break;
    case CollaborationPattern::PARALLEL:
        result = execute_parallel_collaboration(group, input_data);
        break;
    case CollaborationPattern::PIPELINE:
        result = execute_pipeline_collaboration(group, input_data);
        break;
    case CollaborationPattern::CONSENSUS:
        result = execute_consensus_collaboration(group, input_data);
        break;
    case CollaborationPattern::HIERARCHY:
        result = execute_hierarchy_collaboration(group, input_data);
        break;
    case CollaborationPattern::NEGOTIATION:
        result = execute_negotiation_collaboration(group, input_data);
        break;
    }    // Store result in group's shared context
    auto& mutable_group = const_cast<CollaborationGroup&>(group);
    mutable_group.shared_context.clear();
    // Copy the result data to shared context with proper key naming
    mutable_group.shared_context["result"] = result;
    
    return true;
}

std::map<std::string, int> AgentOrchestrator::get_orchestration_metrics() const {
    std::map<std::string, int> metrics;
    metrics["active_workflows"] = active_workflows.load();
    metrics["completed_workflows"] = completed_workflows.load();
    metrics["failed_workflows"] = failed_workflows.load();
    metrics["total_workflows"] = static_cast<int>(workflows.size());
    metrics["collaboration_groups"] = static_cast<int>(collaboration_groups.size());
    return metrics;
}

std::string AgentOrchestrator::get_orchestrator_status() const {
    return orchestrator_running.load() ? "running" : "stopped";
}

// Private methods implementation

void AgentOrchestrator::orchestrator_worker() {
    while (orchestrator_running.load()) {
        std::unique_lock<std::mutex> lock(workflow_mutex);
        workflow_cv.wait(lock, [this] { return !workflow_queue.empty() || !orchestrator_running.load(); });
        
        if (!orchestrator_running.load()) {
            break;
        }
        
        if (!workflow_queue.empty()) {
            std::string workflow_id = workflow_queue.front();
            workflow_queue.pop();
            
            auto it = workflows.find(workflow_id);
            if (it != workflows.end()) {
                lock.unlock();
                
                active_workflows.fetch_add(1);
                
                // Execute workflow
                AgentData empty_context; // TODO: Get context from somewhere
                WorkflowResult result = execute_workflow_internal(it->second, empty_context);
                
                lock.lock();
                workflow_results[workflow_id] = std::move(result);
                
                active_workflows.fetch_sub(1);
                if (workflow_results[workflow_id].success) {
                    completed_workflows.fetch_add(1);
                } else {
                    failed_workflows.fetch_add(1);
                }
            }
        }
    }
}

AgentOrchestrator::WorkflowResult AgentOrchestrator::execute_workflow_internal(const Workflow& workflow, const AgentData& input_context) {
    WorkflowResult result;
    result.workflow_id = workflow.workflow_id;
    result.start_time = std::chrono::system_clock::now();    try {
        // Start with input context as base
        AgentData merged_context = input_context;
        
        // Add workflow global context values
        for (const auto& [key, context_data] : workflow.global_context) {
            // If input context doesn't have this key, add it from global context
            if (!merged_context.has_key(key)) {
                // Use a simple key mapping to avoid complex nested structures
                merged_context.set(key + "_context", context_data.get_string("value", ""));
            }
        }
        
        std::map<std::string, FunctionResult> completed_steps;
        std::vector<WorkflowStep> remaining_steps = workflow.steps;
        
        while (!remaining_steps.empty() && result.success != false) {
            std::vector<WorkflowStep> ready_steps;
            
            // Find steps with satisfied dependencies
            for (auto it = remaining_steps.begin(); it != remaining_steps.end();) {
                if (check_step_dependencies(*it, completed_steps)) {
                    ready_steps.push_back(*it);
                    it = remaining_steps.erase(it);
                } else {
                    ++it;
                }
            }
            
            if (ready_steps.empty()) {
                result.error_message = "Circular dependency detected or missing dependencies";
                result.success = false;
                break;
            }
            
            // Execute ready steps (parallel execution for steps that allow it)
            std::vector<std::future<std::pair<std::string, FunctionResult>>> futures;
            
            for (const auto& step : ready_steps) {
                if (step.parallel_allowed) {
                    futures.push_back(std::async(std::launch::async, [this, &step, &merged_context]() {
                        FunctionResult step_result;
                        execute_workflow_step(step, merged_context, step_result);
                        return std::make_pair(step.step_id, step_result);
                    }));
                } else {
                    FunctionResult step_result;
                    execute_workflow_step(step, merged_context, step_result);
                    completed_steps[step.step_id] = step_result;
                    result.step_results[step.step_id] = step_result;
                }
            }
            
            // Collect results from parallel execution
            for (auto& future : futures) {
                auto [step_id, step_result] = future.get();
                completed_steps[step_id] = step_result;
                result.step_results[step_id] = step_result;
            }
            
            // Check if any step failed
            for (const auto& [step_id, step_result] : completed_steps) {
                if (!step_result.success) {
                    result.success = false;
                    result.error_message = "Step " + step_id + " failed: " + step_result.error_message;
                    break;
                }
            }
        }
        
        if (remaining_steps.empty() && result.error_message.empty()) {
            result.success = true;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Workflow execution error: ") + e.what();
    }
    
    result.end_time = std::chrono::system_clock::now();
    result.total_execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(result.end_time - result.start_time).count();
    
    return result;
}

bool AgentOrchestrator::execute_workflow_step(const WorkflowStep& step, const AgentData& context, FunctionResult& result) {
    auto agent = agent_manager->get_agent(step.agent_id);
    if (!agent) {
        result.success = false;
        result.error_message = "Agent " + step.agent_id + " not found";
        return false;
    }
    
    AgentData step_context = merge_context(context, step.parameters);
    result = agent->execute_function(step.function_name, step_context);
    
    return result.success;
}

bool AgentOrchestrator::check_step_dependencies(const WorkflowStep& step, const std::map<std::string, FunctionResult>& completed_steps) {
    for (const auto& dep_id : step.dependencies) {
        auto it = completed_steps.find(dep_id);
        if (it == completed_steps.end() || !it->second.success) {
            return false;
        }
    }
    return true;
}

AgentData AgentOrchestrator::merge_context(const AgentData& global_context, const AgentData& step_context) {
    AgentData merged = global_context;
    for (const auto& [key, value] : step_context.get_data()) {
        merged.set(key, value);
    }
    return merged;
}

// Collaboration pattern implementations
AgentData AgentOrchestrator::execute_sequential_collaboration(const CollaborationGroup& group, const AgentData& input_data) {
    AgentData current_data = input_data;
    
    for (const auto& agent_id : group.agent_ids) {
        auto agent = agent_manager->get_agent(agent_id);
        if (agent) {
            // Execute a default function or send a message
            auto result = agent->execute_function("process", current_data);
            if (result.success) {
                current_data = result.result_data;
            }
        }
    }
    
    return current_data;
}

AgentData AgentOrchestrator::execute_parallel_collaboration(const CollaborationGroup& group, const AgentData& input_data) {
    std::vector<std::future<FunctionResult>> futures;
    
    for (const auto& agent_id : group.agent_ids) {
        futures.push_back(std::async(std::launch::async, [this, &agent_id, &input_data]() {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent) {
                return agent->execute_function("process", input_data);
            }
            return FunctionResult(false, "Agent not found");
        }));
    }
    
    std::vector<FunctionResult> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    
    // Use result aggregator if available
    if (group.result_aggregator) {
        return group.result_aggregator(results);
    }
    
    // Default aggregation: combine all successful results
    AgentData aggregated_result;
    int success_count = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].success) {
            aggregated_result.set("result_" + std::to_string(success_count), results[i].result_data);
            success_count++;
        }
    }
    aggregated_result.set("success_count", success_count);
    
    return aggregated_result;
}

AgentData AgentOrchestrator::execute_pipeline_collaboration(const CollaborationGroup& group, const AgentData& input_data) {
    // Similar to sequential but with explicit data passing
    return execute_sequential_collaboration(group, input_data);
}

AgentData AgentOrchestrator::execute_consensus_collaboration(const CollaborationGroup& group, const AgentData& input_data) {
    auto parallel_result = execute_parallel_collaboration(group, input_data);
    
    // Simple consensus: majority vote
    // TODO: Implement proper consensus mechanism
    
    return parallel_result;
}

AgentData AgentOrchestrator::execute_hierarchy_collaboration(const CollaborationGroup& group, const AgentData& input_data) {
    if (group.agent_ids.empty()) {
        return AgentData();
    }
    
    // First agent is master, others are slaves
    std::string master_id = group.agent_ids[0];
    auto master_agent = agent_manager->get_agent(master_id);
    
    if (!master_agent) {
        AgentData error_result;
        error_result.set("error", "Master agent not found");
        return error_result;
    }
    
    // Master coordinates the work
    auto result = master_agent->execute_function("coordinate", input_data);
    return result.result_data;
}

AgentData AgentOrchestrator::execute_negotiation_collaboration(const CollaborationGroup& group, const AgentData& input_data) {
    // Implement negotiation rounds
    AgentData current_proposal = input_data;
    
    for (int round = 0; round < group.max_negotiation_rounds; ++round) {
        std::vector<AgentData> agent_responses;
        
        for (const auto& agent_id : group.agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent) {
                auto result = agent->execute_function("negotiate", current_proposal);
                if (result.success) {
                    agent_responses.push_back(result.result_data);
                }
            }
        }
        
        // Check for consensus or update proposal
        // TODO: Implement proper negotiation logic
        if (!agent_responses.empty()) {
            current_proposal = agent_responses[0]; // Simplified
        }
    }
    
    return current_proposal;
}

AgentData AgentOrchestrator::get_collaboration_result(const std::string& group_id) {
    std::lock_guard<std::mutex> lock(collaboration_mutex);
    
    auto it = collaboration_groups.find(group_id);
    if (it != collaboration_groups.end()) {
        // Return the first result if available
        if (!it->second.shared_context.empty()) {
            return it->second.shared_context.begin()->second;
        }
    }
    
    AgentData empty_result;
    empty_result.set("error", "Collaboration group not found or no results available");
    return empty_result;
}

bool AgentOrchestrator::remove_collaboration_group(const std::string& group_id) {
    std::lock_guard<std::mutex> lock(collaboration_mutex);
    
    auto it = collaboration_groups.find(group_id);
    if (it == collaboration_groups.end()) {
        return false;
    }
    
    collaboration_groups.erase(it);
    ServerLogger::logInfo("Removed collaboration group: %s", group_id.c_str());
    return true;
}

std::vector<std::string> AgentOrchestrator::list_collaboration_groups() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(collaboration_mutex));
    
    std::vector<std::string> group_ids;
    for (const auto& [id, group] : collaboration_groups) {
        group_ids.push_back(id);
    }
    return group_ids;
}

bool AgentOrchestrator::coordinate_agents(const std::vector<std::string>& agent_ids, const std::string& coordination_type, const AgentData& parameters) {
    if (coordination_type == "sequential") {
        CollaborationGroup group;
        group.group_id = generate_group_id();
        group.name = "Auto-generated coordination group";
        group.pattern = CollaborationPattern::SEQUENTIAL;
        group.agent_ids = agent_ids;
        
        create_collaboration_group(group);
        return execute_collaboration(group.group_id, "coordination", parameters);
    } else if (coordination_type == "parallel") {
        CollaborationGroup group;
        group.group_id = generate_group_id();
        group.name = "Auto-generated coordination group";
        group.pattern = CollaborationPattern::PARALLEL;
        group.agent_ids = agent_ids;
        
        create_collaboration_group(group);
        return execute_collaboration(group.group_id, "coordination", parameters);
    }
    
    ServerLogger::logError("Unknown coordination type: %s", coordination_type.c_str());
    return false;
}

bool AgentOrchestrator::setup_agent_pipeline(const std::vector<std::string>& agent_ids, const std::string& pipeline_name) {
    CollaborationGroup group;
    group.group_id = pipeline_name;
    group.name = pipeline_name;
    group.pattern = CollaborationPattern::PIPELINE;
    group.agent_ids = agent_ids;
    
    return create_collaboration_group(group);
}

bool AgentOrchestrator::execute_pipeline(const std::string& pipeline_name, const AgentData& input_data) {
    return execute_collaboration(pipeline_name, "pipeline execution", input_data);
}

std::vector<std::string> AgentOrchestrator::get_active_workflows() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(workflow_mutex));
    
    std::vector<std::string> active_ids;
    // This is a simplified implementation - in a real system you'd track which workflows are currently executing
    if (active_workflows.load() > 0) {
        for (const auto& [id, workflow] : workflows) {
            active_ids.push_back(id);
            if (active_ids.size() >= static_cast<size_t>(active_workflows.load())) {
                break;
            }
        }
    }
    return active_ids;
}

std::string AgentOrchestrator::select_optimal_agent(const std::string& capability, const AgentData& context) {
    auto agents = get_agents_by_capability(capability);
    if (agents.empty()) {
        return "";
    }
    
    // Simple load-based selection - select agent with lowest load
    std::string best_agent = agents[0];
    double lowest_load = calculate_agent_load(best_agent);
    
    for (size_t i = 1; i < agents.size(); ++i) {
        double load = calculate_agent_load(agents[i]);
        if (load < lowest_load) {
            lowest_load = load;
            best_agent = agents[i];
        }
    }
    
    return best_agent;
}

bool AgentOrchestrator::distribute_workload(const std::string& task_type, const std::vector<AgentData>& tasks) {
    auto agents = get_agents_by_capability(task_type);
    if (agents.empty()) {
        ServerLogger::logError("No agents found for task type: %s", task_type.c_str());
        return false;
    }
    
    // Simple round-robin distribution
    for (size_t i = 0; i < tasks.size(); ++i) {
        std::string agent_id = agents[i % agents.size()];
        auto agent = agent_manager->get_agent(agent_id);
        if (agent) {
            // Execute task asynchronously
            std::thread([agent, task_type, task_data = tasks[i]]() {
                agent->execute_function(task_type, task_data);
            }).detach();
        }
    }
    
    return true;
}

void AgentOrchestrator::optimize_agent_allocation() {
    // Simple optimization: redistribute load if any agent is overloaded
    auto agents = agent_manager->list_agents();
    
    std::map<std::string, double> agent_loads;
    double total_load = 0.0;
    
    for (const auto& agent_id : agents) {
        double load = calculate_agent_load(agent_id);
        agent_loads[agent_id] = load;
        total_load += load;
    }
    
    if (agents.empty()) return;
    
    double average_load = total_load / agents.size();
    
    // Log optimization information
    for (const auto& [agent_id, load] : agent_loads) {
        if (load > average_load * 1.5) {
            ServerLogger::logWarning("Agent %s is overloaded: %.2f (avg: %.2f)", 
                                   agent_id.c_str(), load, average_load);
        }
    }
}

double AgentOrchestrator::calculate_agent_load(const std::string& agent_id) {
    // Simple load calculation based on active workflows
    // In a real implementation, this would consider actual agent metrics
    auto agent = agent_manager->get_agent(agent_id);
    if (!agent) {
        return 0.0;
    }
    
    // Return a mock load value - replace with actual implementation
    return static_cast<double>(active_workflows.load()) / 10.0;
}

std::vector<std::string> AgentOrchestrator::get_agents_by_capability(const std::string& capability) {
    auto all_agents = agent_manager->list_agents();
    std::vector<std::string> capable_agents;
    
    for (const auto& agent_id : all_agents) {
        auto agent = agent_manager->get_agent(agent_id);
        if (agent) {
            // Check if agent has the required capability
            // This is a simplified check - in a real system you'd check agent capabilities/functions
            capable_agents.push_back(agent_id);
        }
    }
      return capable_agents;
}

std::string AgentOrchestrator::generate_group_id() {
    // Generate a unique group ID based on timestamp and counter
    static std::atomic<int> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    return "group_" + std::to_string(timestamp) + "_" + std::to_string(counter.fetch_add(1));
}

} // namespace kolosal::agents

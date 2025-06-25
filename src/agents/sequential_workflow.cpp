#include "kolosal/agents/sequential_workflow.hpp"
#include "kolosal/agents/server_logger_adapter.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <future>
#include <fstream>
#include <thread>
#include <optional>
#include <set>
#include <ctime>
#include <unordered_map>

namespace kolosal::agents {

// SequentialWorkflowExecutor implementation
SequentialWorkflowExecutor::SequentialWorkflowExecutor(std::shared_ptr<YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
    // Initialize logger
    logger = std::make_shared<ServerLoggerAdapter>();
    logger->info("Sequential workflow executor initialized");
}

SequentialWorkflowExecutor::~SequentialWorkflowExecutor() {
    // Cancel all active workflows
    std::lock_guard<std::mutex> lock(workflow_mutex);
    std::lock_guard<std::mutex> flag_lock(cancellation_flags_mutex);
    for (auto& [workflow_id, flag] : workflow_cancellation_flags) {
        flag->store(true);
    }
    logger->info("Sequential workflow executor destroyed");
}

bool SequentialWorkflowExecutor::register_workflow(const SequentialWorkflow& workflow) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    if (workflows.find(workflow.workflow_id) != workflows.end()) {
        logger->warn("Workflow already exists: " + workflow.workflow_id);
        return false;
    }
    
    // Validate workflow before registration
    if (!validate_workflow(workflow)) {
        logger->error("Invalid workflow configuration: " + workflow.workflow_id);
        return false;
    }
    
    workflows[workflow.workflow_id] = workflow;
    
    // Create cancellation flag thread-safely
    {
        std::lock_guard<std::mutex> flag_lock(cancellation_flags_mutex);
        workflow_cancellation_flags[workflow.workflow_id] = std::make_unique<std::atomic<bool>>(false);
    }
    
    logger->info("Registered sequential workflow: " + workflow.workflow_id + 
                " (" + std::to_string(workflow.steps.size()) + " steps)");
    return true;
}

bool SequentialWorkflowExecutor::remove_workflow(const std::string& workflow_id) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it == workflows.end()) {
        return false;
    }
    
    // Cancel if running
    {
        std::lock_guard<std::mutex> flag_lock(cancellation_flags_mutex);
        auto flag_it = workflow_cancellation_flags.find(workflow_id);
        if (flag_it != workflow_cancellation_flags.end()) {
            flag_it->second->store(true);
        }
        workflow_cancellation_flags.erase(workflow_id);
    }
    
    workflows.erase(it);
    
    {
        std::lock_guard<std::mutex> results_lock(results_mutex);
        workflow_results.erase(workflow_id);
    }
    
    logger->info("Removed workflow: " + workflow_id);
    return true;
}

std::vector<std::string> SequentialWorkflowExecutor::list_workflows() const {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    std::vector<std::string> workflow_ids;
    for (const auto& [id, workflow] : workflows) {
        workflow_ids.push_back(id);
    }
    return workflow_ids;
}

std::optional<SequentialWorkflow> SequentialWorkflowExecutor::get_workflow(const std::string& workflow_id) const {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it != workflows.end()) {
        return it->second;
    }
    return std::nullopt;
}

SequentialWorkflowResult SequentialWorkflowExecutor::execute_workflow(const std::string& workflow_id, 
                                                                     const AgentData& input_context) {
    std::lock_guard<std::mutex> lock(workflow_mutex);
    
    auto it = workflows.find(workflow_id);
    if (it == workflows.end()) {
        SequentialWorkflowResult result;
        result.workflow_id = workflow_id;
        result.success = false;
        result.error_message = "Workflow not found: " + workflow_id;
        return result;
    }
    
    logger->info("Executing sequential workflow: " + workflow_id);
    active_workflows++;
    
    SequentialWorkflowResult result = execute_workflow_internal(it->second, input_context);
    
    {
        std::lock_guard<std::mutex> results_lock(results_mutex);
        workflow_results[workflow_id] = result;
    }
    
    active_workflows--;
    if (result.success) {
        completed_workflows++;
    } else {
        failed_workflows++;
    }
    
    update_workflow_metrics(result);
    
    // Call completion callback if available
    if (it->second.on_workflow_complete) {
        try {
            it->second.on_workflow_complete(result);
        } catch (const std::exception& e) {
            logger->error("Error in workflow completion callback: " + std::string(e.what()));
        }
    }
    
    return result;
}

std::string SequentialWorkflowExecutor::execute_workflow_async(const std::string& workflow_id, 
                                                              const AgentData& input_context) {
    // For async execution, we would typically use a thread pool
    // For now, we'll execute synchronously and return immediately
    std::string execution_id = "exec_" + workflow_id + "_" + std::to_string(std::time(nullptr));
    
    // In a full implementation, this would be executed in a separate thread
    std::thread([this, workflow_id, input_context, execution_id]() {
        auto result = execute_workflow(workflow_id, input_context);
        logger->info("Async workflow completed: " + execution_id + " (success: " + 
                    (result.success ? "true" : "false") + ")");
    }).detach();
    
    return execution_id;
}

bool SequentialWorkflowExecutor::cancel_workflow(const std::string& workflow_id) {
    std::lock_guard<std::mutex> flag_lock(cancellation_flags_mutex);
    auto it = workflow_cancellation_flags.find(workflow_id);
    if (it != workflow_cancellation_flags.end()) {
        it->second->store(true);
        logger->info("Cancellation requested for workflow: " + workflow_id);
        return true;
    }
    return false;
}

bool SequentialWorkflowExecutor::pause_workflow(const std::string& workflow_id) {
    // TODO: Implement workflow pausing
    logger->info("Pause requested for workflow: " + workflow_id + " (not implemented)");
    return false;
}

bool SequentialWorkflowExecutor::resume_workflow(const std::string& workflow_id) {
    // TODO: Implement workflow resuming
    logger->info("Resume requested for workflow: " + workflow_id + " (not implemented)");
    return false;
}

std::optional<SequentialWorkflowResult> SequentialWorkflowExecutor::get_workflow_result(const std::string& workflow_id) const {
    std::lock_guard<std::mutex> lock(results_mutex);
    
    auto it = workflow_results.find(workflow_id);
    if (it != workflow_results.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::map<std::string, std::string> SequentialWorkflowExecutor::get_workflow_status(const std::string& workflow_id) const {
    std::map<std::string, std::string> status;
    
    {
        std::lock_guard<std::mutex> lock(workflow_mutex);
        auto workflow_it = workflows.find(workflow_id);
        if (workflow_it == workflows.end()) {
            status["status"] = "not_found";
            return status;
        }
        status["workflow_name"] = workflow_it->second.workflow_name;
        status["total_steps"] = std::to_string(workflow_it->second.steps.size());
    }
    
    // Check cancellation flag
    {
        std::lock_guard<std::mutex> flag_lock(cancellation_flags_mutex);
        auto flag_it = workflow_cancellation_flags.find(workflow_id);
        if (flag_it != workflow_cancellation_flags.end() && flag_it->second->load()) {
            status["status"] = "cancelled";
            return status;
        }
    }
    
    {
        std::lock_guard<std::mutex> results_lock(results_mutex);
        auto result_it = workflow_results.find(workflow_id);
        if (result_it != workflow_results.end()) {
            const auto& result = result_it->second;
            status["status"] = result.success ? "completed" : "failed";
            status["executed_steps"] = std::to_string(result.executed_steps.size());
            status["successful_steps"] = std::to_string(result.successful_steps);
            status["failed_steps"] = std::to_string(result.failed_steps);
            status["execution_time_ms"] = std::to_string(result.total_execution_time_ms);
            if (!result.error_message.empty()) {
                status["error"] = result.error_message;
            }
        } else {
            status["status"] = "registered";
        }
    }
    
    return status;
}

std::map<std::string, int> SequentialWorkflowExecutor::get_executor_metrics() const {
    return {
        {"active_workflows", active_workflows.load()},
        {"completed_workflows", completed_workflows.load()},
        {"failed_workflows", failed_workflows.load()},
        {"total_registered_workflows", static_cast<int>(workflows.size())}
    };
}

SequentialWorkflow SequentialWorkflowExecutor::create_workflow(const std::string& workflow_id, const std::string& name) {
    return SequentialWorkflow(workflow_id, name);
}

SequentialWorkflowStep SequentialWorkflowExecutor::create_step(const std::string& step_id, const std::string& step_name,
                                                              const std::string& agent_id, const std::string& function_name) {
    return SequentialWorkflowStep(step_id, step_name, agent_id, function_name);
}

std::vector<std::string> SequentialWorkflowExecutor::get_workflow_dependencies(const std::string& workflow_id) const {
    // TODO: Implement dependency analysis
    std::vector<std::string> dependencies;
    
    std::lock_guard<std::mutex> lock(workflow_mutex);
    auto it = workflows.find(workflow_id);
    if (it != workflows.end()) {
        // For now, return the agent IDs as dependencies
        std::set<std::string> unique_agents;
        for (const auto& step : it->second.steps) {
            unique_agents.insert(step.agent_id);
        }
        dependencies.assign(unique_agents.begin(), unique_agents.end());
    }
    
    return dependencies;
}

bool SequentialWorkflowExecutor::export_workflow_template(const std::string& workflow_id, const std::string& file_path) const {
    // TODO: Implement workflow template export
    logger->info("Export template requested for workflow: " + workflow_id + " to " + file_path + " (not implemented)");
    return false;
}

bool SequentialWorkflowExecutor::import_workflow_template(const std::string& file_path) {
    // TODO: Implement workflow template import
    logger->info("Import template requested from: " + file_path + " (not implemented)");
    return false;
}

bool SequentialWorkflowExecutor::validate_workflow(const SequentialWorkflow& workflow) const {
    if (workflow.workflow_id.empty() || workflow.steps.empty()) {
        return false;
    }
    
    // Check for duplicate step IDs
    std::set<std::string> step_ids;
    for (const auto& step : workflow.steps) {
        if (step.step_id.empty() || step.agent_id.empty() || step.function_name.empty()) {
            return false;
        }
        
        if (step_ids.find(step.step_id) != step_ids.end()) {
            logger->error("Duplicate step ID found: " + step.step_id);
            return false;
        }
        step_ids.insert(step.step_id);
        
        // Validate that agents exist
        if (!agent_manager->get_agent(step.agent_id)) {
            logger->error("Agent not found for step " + step.step_id + ": " + step.agent_id);
            return false;
        }
    }
    
    return true;
}

// Private execution methods
SequentialWorkflowResult SequentialWorkflowExecutor::execute_workflow_internal(const SequentialWorkflow& workflow, 
                                                                              const AgentData& input_context) {
    SequentialWorkflowResult result;
    result.workflow_id = workflow.workflow_id;
    result.workflow_name = workflow.workflow_name;
    result.start_time = std::chrono::system_clock::now();
    result.initial_context = input_context;
    result.total_steps = static_cast<int>(workflow.steps.size());
    
    // Initialize context with global context and input
    AgentData current_context = workflow.global_context;
    for (const auto& [key, value] : input_context.get_data()) {
        current_context.set(key, value);
    }
    
    logger->info("Starting workflow execution: " + workflow.workflow_id + 
                " with " + std::to_string(workflow.steps.size()) + " steps");
    
    try {
        for (size_t i = 0; i < workflow.steps.size(); ++i) {
            const auto& step = workflow.steps[i];
            
            // Check for cancellation
            {
                std::lock_guard<std::mutex> flag_lock(cancellation_flags_mutex);
                auto flag_it = workflow_cancellation_flags.find(workflow.workflow_id);
                if (flag_it != workflow_cancellation_flags.end() && flag_it->second->load()) {
                    result.error_message = "Workflow cancelled";
                    result.success = false;
                    break;
                }
            }
            
            // Check workflow timeout
            if (check_workflow_timeout(result.start_time, workflow.max_execution_time_seconds)) {
                result.error_message = "Workflow timeout exceeded";
                result.success = false;
                break;
            }
            
            logger->info("Executing step " + std::to_string(i + 1) + "/" + 
                        std::to_string(workflow.steps.size()) + ": " + step.step_name);
            
            auto step_start = std::chrono::high_resolution_clock::now();
            FunctionResult step_result;
            std::string step_error;
            
            bool step_success = execute_step(step, current_context, step_result, step_error);
            
            auto step_end = std::chrono::high_resolution_clock::now();
            double step_time = std::chrono::duration<double, std::milli>(step_end - step_start).count();
            
            // Record step execution
            result.executed_steps.push_back(step.step_id);
            result.step_results[step.step_id] = step_result;
            result.step_execution_times[step.step_id] = step_time;
            
            if (step_success) {
                result.successful_steps++;
                
                // Process step result and update context
                current_context = process_step_result(step, current_context, step_result);
                
                // Call step completion callback
                if (workflow.on_step_complete) {
                    try {
                        workflow.on_step_complete(step.step_id, step_result);
                    } catch (const std::exception& e) {
                        logger->error("Error in step completion callback: " + std::string(e.what()));
                    }
                }
                
                log_step_execution(workflow.workflow_id, step.step_id, step_result, step_time);
            } else {
                result.failed_steps++;
                result.step_errors[step.step_id] = step_error;
                
                // Call step error callback
                if (workflow.on_step_error) {
                    try {
                        workflow.on_step_error(step.step_id, step_error);
                    } catch (const std::exception& e) {
                        logger->error("Error in step error callback: " + std::string(e.what()));
                    }
                }
                
                logger->error("Step failed: " + step.step_id + " - " + step_error);
                
                if (workflow.stop_on_failure && !step.continue_on_failure) {
                    result.error_message = "Step " + step.step_id + " failed: " + step_error;
                    result.success = false;
                    break;
                }
            }
        }
        
        // Set final success state
        if (result.error_message.empty()) {
            result.success = (result.failed_steps == 0 || !workflow.stop_on_failure);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Workflow execution error: " + std::string(e.what());
        logger->error("Workflow execution error: " + std::string(e.what()));
    }
    
    result.end_time = std::chrono::system_clock::now();
    result.total_execution_time_ms = std::chrono::duration<double, std::milli>(
        result.end_time - result.start_time).count();
    result.final_context = current_context;
    
    logger->info("Workflow execution completed: " + workflow.workflow_id + 
                " (success: " + (result.success ? "true" : "false") + 
                ", time: " + std::to_string(result.total_execution_time_ms) + "ms)");
    
    return result;
}

bool SequentialWorkflowExecutor::execute_step(const SequentialWorkflowStep& step, AgentData& context, 
                                             FunctionResult& result, std::string& error_message) {
    // Check preconditions
    if (!validate_step_precondition(step, context)) {
        error_message = "Step precondition failed";
        result = FunctionResult(false, error_message);
        return false;
    }
    
    // Get agent
    auto agent = agent_manager->get_agent(step.agent_id);
    if (!agent) {
        error_message = "Agent not found: " + step.agent_id;
        result = FunctionResult(false, error_message);
        return false;
    }
    
    // Merge step parameters with context
    AgentData execution_context = context;
    for (const auto& [key, value] : step.parameters.get_data()) {
        execution_context.set(key, value);
    }
    
    // Execute with retries
    int attempts = 0;
    while (attempts <= step.max_retries) {
        try {
            result = agent->execute_function(step.function_name, execution_context);
            
            if (result.success && validate_step_result(step, result)) {
                return true;
            }
            
            if (attempts < step.max_retries) {
                attempts++;
                logger->warn("Step " + step.step_id + " attempt " + std::to_string(attempts) + 
                           " failed, retrying...");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 * attempts)); // Exponential backoff
            } else {
                error_message = result.error_message.empty() ? "Step validation failed" : result.error_message;
                return false;
            }
        } catch (const std::exception& e) {
            error_message = "Step execution exception: " + std::string(e.what());
            if (attempts >= step.max_retries) {
                result = FunctionResult(false, error_message);
                return false;
            }
            attempts++;
        }
    }
    
    return false;
}

bool SequentialWorkflowExecutor::validate_step_precondition(const SequentialWorkflowStep& step, const AgentData& context) {
    if (step.precondition) {
        try {
            return step.precondition(context);
        } catch (const std::exception& e) {
            logger->error("Error in step precondition: " + std::string(e.what()));
            return false;
        }
    }
    return true;
}

bool SequentialWorkflowExecutor::validate_step_result(const SequentialWorkflowStep& step, const FunctionResult& result) {
    if (step.validation) {
        try {
            return step.validation(result);
        } catch (const std::exception& e) {
            logger->error("Error in step validation: " + std::string(e.what()));
            return false;
        }
    }
    return result.success;
}

AgentData SequentialWorkflowExecutor::process_step_result(const SequentialWorkflowStep& step, const AgentData& context, 
                                                         const FunctionResult& result) {
    if (step.result_processor) {
        try {
            return step.result_processor(context, result);
        } catch (const std::exception& e) {
            logger->error("Error in step result processor: " + std::string(e.what()));
        }
    }
    
    // Default processing: merge result data into context
    AgentData updated_context = context;
    for (const auto& [key, value] : result.result_data.get_data()) {
        updated_context.set(key, value);
    }
    
    return updated_context;
}

void SequentialWorkflowExecutor::log_step_execution(const std::string& workflow_id, const std::string& step_id, 
                                                   const FunctionResult& result, double execution_time) {
    logger->info("Step completed - Workflow: " + workflow_id + 
                ", Step: " + step_id + 
                ", Success: " + (result.success ? "true" : "false") + 
                ", Time: " + std::to_string(execution_time) + "ms");
}

void SequentialWorkflowExecutor::update_workflow_metrics(const SequentialWorkflowResult& result) {
    // This could be used to update monitoring/metrics systems
    logger->debug("Workflow metrics - ID: " + result.workflow_id + 
                 ", Total Steps: " + std::to_string(result.total_steps) + 
                 ", Successful: " + std::to_string(result.successful_steps) + 
                 ", Failed: " + std::to_string(result.failed_steps) + 
                 ", Total Time: " + std::to_string(result.total_execution_time_ms) + "ms");
}

bool SequentialWorkflowExecutor::check_workflow_timeout(const std::chrono::time_point<std::chrono::system_clock>& start_time,
                                                       int max_seconds) {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    return elapsed > max_seconds;
}

// SequentialWorkflowBuilder implementation
SequentialWorkflowBuilder::SequentialWorkflowBuilder(const std::string& workflow_id, const std::string& name) {
    workflow.workflow_id = workflow_id;
    workflow.workflow_name = name;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::description(const std::string& desc) {
    workflow.description = desc;
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::global_context(const AgentData& context) {
    workflow.global_context = context;
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::stop_on_failure(bool stop) {
    workflow.stop_on_failure = stop;
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::max_execution_time(int seconds) {
    workflow.max_execution_time_seconds = seconds;
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::metadata(const std::string& key, const std::string& value) {
    workflow.metadata[key] = value;
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::add_step(const SequentialWorkflowStep& step) {
    workflow.steps.push_back(step);
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::add_step(const std::string& step_id, const std::string& step_name,
                                                              const std::string& agent_id, const std::string& function_name) {
    SequentialWorkflowStep step(step_id, step_name, agent_id, function_name);
    workflow.steps.push_back(step);
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::add_step(const std::string& step_id, const std::string& step_name,
                                                              const std::string& agent_id, const std::string& function_name,
                                                              const AgentData& parameters) {
    SequentialWorkflowStep step(step_id, step_name, agent_id, function_name);
    step.parameters = parameters;
    workflow.steps.push_back(step);
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::step_timeout(int seconds) {
    if (!workflow.steps.empty()) {
        workflow.steps.back().timeout_seconds = seconds;
    }
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::step_retries(int retries) {
    if (!workflow.steps.empty()) {
        workflow.steps.back().max_retries = retries;
    }
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::step_continue_on_failure(bool continue_on_fail) {
    if (!workflow.steps.empty()) {
        workflow.steps.back().continue_on_failure = continue_on_fail;
    }
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::step_precondition(std::function<bool(const AgentData&)> condition) {
    if (!workflow.steps.empty()) {
        workflow.steps.back().precondition = condition;
    }
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::step_validation(std::function<bool(const FunctionResult&)> validation) {
    if (!workflow.steps.empty()) {
        workflow.steps.back().validation = validation;
    }
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::step_processor(std::function<AgentData(const AgentData&, const FunctionResult&)> processor) {
    if (!workflow.steps.empty()) {
        workflow.steps.back().result_processor = processor;
    }
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::on_step_complete(std::function<void(const std::string&, const FunctionResult&)> callback) {
    workflow.on_step_complete = callback;
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::on_step_error(std::function<void(const std::string&, const std::string&)> callback) {
    workflow.on_step_error = callback;
    return *this;
}

SequentialWorkflowBuilder& SequentialWorkflowBuilder::on_workflow_complete(std::function<void(const SequentialWorkflowResult&)> callback) {
    workflow.on_workflow_complete = callback;
    return *this;
}

SequentialWorkflow SequentialWorkflowBuilder::build() {
    return std::move(workflow);
}

} // namespace kolosal::agents

// File: src/agents/agent_core.cpp
#include "kolosal/agents/agent_core.hpp"
#include "kolosal/agents/builtin_functions.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/agents/server_logger_adapter.hpp"
#include <mutex>

namespace kolosal::agents {

AgentCore::AgentCore(const std::string& name, const std::string& type, AgentRole role)
    : agent_id(UUIDGenerator::generate()), 
      agent_name(name.empty() ? "Agent-" + agent_id.substr(0, 8) : name), 
      agent_type(type),
      current_role(role) {
    
    // Create logger bridge
    logger = std::make_shared<ServerLoggerAdapter>();
    
    // Initialize role manager
    role_manager = std::make_shared<AgentRoleManager>();
    
    // Initialize components
    function_manager = std::make_shared<FunctionManager>(logger);
    job_manager = std::make_shared<JobManager>(function_manager, logger);
    event_system = std::make_shared<EventSystem>(logger);
    tool_registry = std::make_shared<ToolRegistry>(logger);
    memory_manager = std::make_shared<MemoryManager>(agent_id, logger);
    planning_coordinator = std::make_shared<PlanningReasoningCoordinator>(logger);
    
    // Set up role-based capabilities and functions
    set_role(role);
    // Register default functions
    function_manager->register_function(std::make_unique<AddFunction>());
    function_manager->register_function(std::make_unique<EchoFunction>());    
    function_manager->register_function(std::make_unique<DelayFunction>());
    function_manager->register_function(std::make_unique<TextAnalysisFunction>());
    function_manager->register_function(std::make_unique<TextProcessingFunction>());
    function_manager->register_function(std::make_unique<DataTransformFunction>());
    function_manager->register_function(std::make_unique<DataAnalysisFunction>());
    function_manager->register_function(std::make_unique<InferenceFunction>());
    function_manager->register_function(std::make_unique<RetrievalFunction>());
    function_manager->register_function(std::make_unique<ContextRetrievalFunction>());
    function_manager->register_function(std::make_unique<ToolDiscoveryFunction>(function_manager));
    function_manager->register_function(std::make_unique<WebSearchFunction>());
    function_manager->register_function(std::make_unique<CodeGenerationFunction>());
    
    // Register document management functions
    function_manager->register_function(std::make_unique<AddDocumentFunction>());
    function_manager->register_function(std::make_unique<RemoveDocumentFunction>());
    
    // Register document parsing functions
    function_manager->register_function(std::make_unique<ParsePdfFunction>());
    function_manager->register_function(std::make_unique<ParseDocxFunction>());
    
    // Register embedding and utility functions
    function_manager->register_function(std::make_unique<GetEmbeddingFunction>());
    function_manager->register_function(std::make_unique<TestDocumentServiceFunction>());
    
    logger->info("Agent created: " + agent_name + " (ID: " + agent_id.substr(0, 8) + "...) with role: " + 
                role_manager->role_to_string(current_role));
}

void AgentCore::set_role(AgentRole role) {
    current_role = role;
    
    try {
        const auto& role_definition = role_manager->get_role_definition(role);
        
        // Clear existing capabilities and add role-based ones
        {
            std::lock_guard<std::mutex> lock(capabilities_mutex);
            capabilities.clear();
            for (const auto& capability : role_definition.capabilities) {
                capabilities.push_back(capability.name);
            }
        }
        
        // Set specializations
        specializations = role_definition.specializations;
        
        // Register default functions for this role
        for (const std::string& func_name : role_definition.default_functions) {
            // Functions are already registered in the constructor, 
            // this is just for role-specific function selection
            logger->debug("Role-specific function available: " + func_name);
        }
        
        logger->info("Agent role set to: " + role_manager->role_to_string(role));
        
    } catch (const std::exception& e) {
        logger->error("Failed to set agent role: " + std::string(e.what()));
    }
}

void AgentCore::add_specialization(AgentSpecialization spec) {
    auto it = std::find(specializations.begin(), specializations.end(), spec);
    if (it == specializations.end()) {
        specializations.push_back(spec);
        logger->debug("Added specialization: " + role_manager->specialization_to_string(spec));
    }
}

FunctionResult AgentCore::execute_tool(const std::string& tool_name, const AgentData& params) {
    if (!tool_registry) {
        return FunctionResult(false, "Tool registry not initialized");
    }
    
    ToolContext context(agent_id);
    context.logger = logger;
    
    return tool_registry->execute_tool(tool_name, params, context);
}

ExecutionPlan AgentCore::create_plan(const std::string& goal, const std::string& context) {
    if (!planning_coordinator) {
        throw std::runtime_error("Planning coordinator not initialized");
    }
    
    std::vector<std::string> available_functions = function_manager->get_function_names();
    return planning_coordinator->create_intelligent_plan(goal, context, available_functions);
}

bool AgentCore::execute_plan(const std::string& plan_id) {
    if (!planning_coordinator) {
        return false;
    }
    
    auto* planning_system = planning_coordinator->get_planning_system();
    if (!planning_system) {
        return false;
    }
    
    ExecutionPlan* plan = planning_system->get_plan(plan_id);
    if (!plan) {
        logger->error("Plan not found: " + plan_id);
        return false;
    }
    
    logger->info("Executing plan: " + plan->name);
    
    while (!plan->is_complete()) {
        auto next_tasks = planning_system->get_next_tasks(plan_id);
        
        if (next_tasks.empty()) {
            // Check if we're stuck due to failed dependencies
            auto failed_tasks = plan->get_tasks_by_status(TaskStatus::FAILED);
            if (!failed_tasks.empty()) {
                logger->error("Plan execution blocked by failed tasks");
                return false;
            }
            break; // No more tasks to execute
        }
        
        // Execute ready tasks
        for (Task* task : next_tasks) {
            logger->debug("Executing task: " + task->name);
            
            planning_system->update_task_status(plan_id, task->id, TaskStatus::IN_PROGRESS);
            
            FunctionResult result = execute_function(task->function_name, task->parameters);
            
            if (result.success) {
                planning_system->set_task_result(plan_id, task->id, result.result_data);
                planning_system->update_task_status(plan_id, task->id, TaskStatus::COMPLETED);
                logger->debug("Task completed: " + task->name);
            } else {
                planning_system->update_task_status(plan_id, task->id, TaskStatus::FAILED, result.error_message);
                logger->error("Task failed: " + task->name + " - " + result.error_message);
                
                if (task->retry_count < task->max_retries) {
                    task->retry_count++;
                    planning_system->update_task_status(plan_id, task->id, TaskStatus::PENDING);
                    logger->info("Retrying task: " + task->name + " (attempt " + std::to_string(task->retry_count + 1) + ")");
                }
            }
        }
    }
    
    bool success = plan->is_complete();
    std::string status_msg = success ? "completed successfully" : "failed";
    logger->info("Plan execution " + status_msg + ": " + plan->name);
    return success;
}

std::string AgentCore::reason_about(const std::string& question, const std::string& context) {
    if (!planning_coordinator) {
        return "Reasoning system not available";
    }
    
    auto* reasoning_system = planning_coordinator->get_reasoning_system();
    if (!reasoning_system) {
        return "Reasoning system not initialized";
    }
    
    return reasoning_system->reason_about(question, context);
}

void AgentCore::store_memory(const std::string& content, const std::string& type) {
    if (memory_manager) {
        if (type == "conversation") {
            memory_manager->store_conversation("agent", content);
        } else if (type == "fact") {
            memory_manager->store_fact(content);
        } else {
            // Store as general memory
            std::string id = "mem_" + std::to_string(std::time(nullptr));
            MemoryEntry entry(id, content, type);
            entry.metadata["agent_id"] = agent_id;
            memory_manager->get_vector_memory()->store(entry);
        }
    }
}

std::vector<MemoryEntry> AgentCore::recall_memories(const std::string& query, int max_results) {
    if (memory_manager) {
        return memory_manager->retrieve_relevant_memories(query, max_results);
    }
    return {};
}

void AgentCore::set_working_context(const std::string& key, const AgentData& data) {
    if (memory_manager) {
        memory_manager->get_working_memory()->set_context(key, data);
    }
}

AgentData AgentCore::get_working_context(const std::string& key) {
    if (memory_manager) {
        return memory_manager->get_working_memory()->get_context(key);
    }
    return AgentData();
}

std::vector<std::string> AgentCore::discover_tools(const ToolFilter& filter) {
    if (tool_registry) {
        return tool_registry->discover_tools(filter);
    }
    return {};
}

bool AgentCore::register_custom_tool(std::unique_ptr<Tool> tool) {
    if (tool_registry) {
        return tool_registry->register_tool(std::move(tool));
    }
    return false;
}

ToolSchema AgentCore::get_tool_schema(const std::string& tool_name) {
    if (tool_registry) {
        return tool_registry->get_tool_schema(tool_name);
    }
    throw std::runtime_error("Tool registry not available");
}

AgentCore::AgentStats AgentCore::get_statistics() const {
    AgentStats stats = {};
    
    if (memory_manager) {
        auto memory_stats = memory_manager->get_statistics();
        stats.memory_entries_count = memory_stats.conversation_count + memory_stats.vector_memory_count;
    }
    
    if (planning_coordinator) {
        auto planning_stats = planning_coordinator->get_planning_system()->get_statistics();
        stats.total_plans_created = planning_stats.active_plans + planning_stats.completed_plans;
    }
    
    stats.last_activity = std::chrono::system_clock::now();
    return stats;
}

AgentCore::~AgentCore() = default;

void AgentCore::start() {
    running.store(true);
    if (logger) logger->info("Agent started: " + agent_name);
}

void AgentCore::stop() {
    running.store(false);
    if (logger) logger->info("Agent stopped: " + agent_name);
}

void AgentCore::set_message_router(std::shared_ptr<MessageRouter> router) {
    message_router = router;
    if (logger) logger->debug("Message router set for agent: " + agent_name);
}

FunctionResult AgentCore::execute_function(const std::string& name, const AgentData& params) {
    if (!function_manager) {
        FunctionResult result;
        result.success = false;
        result.error_message = "Function manager not initialized";
        result.execution_time_ms = 0.0;
        // result.result_data is default
        return result;
    }
    return function_manager->execute_function(name, params);
}

void AgentCore::send_message(const std::string& to_agent, const std::string& message_type, const AgentData& payload) {
    if (message_router) {
        AgentMessage msg(agent_id, to_agent, message_type);
        msg.id = UUIDGenerator::generate();
        msg.payload = payload;
        msg.timestamp = std::chrono::system_clock::now();
        message_router->route_message(msg);
    } else if (logger) {
        logger->warn("No message router set for agent: " + agent_name);
    }
}

void AgentCore::add_capability(const std::string& capability) {
    if (capability.empty()) {
        logger->warn("Attempted to add empty capability to " + agent_name);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(capabilities_mutex);
        // Check for duplicate capability
        if (std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end()) {
            logger->debug("Capability already exists in " + agent_name + ": " + capability);
            return;
        }
        capabilities.push_back(capability);
    }
    
    logger->debug("Capability added to " + agent_name + ": " + capability);
}

void AgentCore::handle_message(const AgentMessage& message) {
    if (!running.load()) {
        logger->warn("Ignoring message - agent is not running: " + agent_name);
        return;
    }

    std::lock_guard<std::mutex> lock(message_mutex);

    if (message.from_agent.empty()) {
        logger->warn("Received message with empty sender ID");
        return;
    }

    logger->debug("Received message: " + message.type + " from " + message.from_agent);
    
    try {
        // Handle standard message types
        if (message.type == "ping") {
            AgentData pong_data;
            pong_data.set("timestamp", std::to_string(std::time(nullptr)));
            send_message(message.from_agent, "pong", pong_data);
        } else if (message.type == "greeting") {
            auto greeting_msg = message.payload.get_string("message");
            if (!greeting_msg.empty()) {
                logger->info("Greeting received: " + greeting_msg);
            }
        } else if (message.type == "function_request") {
            // Handle function request inline since it's private
            std::string function_name = message.payload.get_string("function");
            if (function_name.empty()) {
                throw std::runtime_error("Missing function name in function request");
            }

            FunctionResult result = execute_function(function_name, message.payload);
            
            AgentData response_data;
            response_data.set("success", result.success);
            response_data.set("error_message", result.error_message);
            response_data.set("execution_time_ms", result.execution_time_ms);
            response_data.set("result_data", result.result_data);
            
            send_message(message.from_agent, "function_response", response_data);
        }
        
        // Emit message received event
        AgentData event_data;
        event_data.set("agent_id", agent_id);
        event_data.set("from_agent", message.from_agent);
        event_data.set("message_type", message.type);
        event_system->emit("message_received", agent_id, event_data);
    } catch (const std::exception& e) {
        logger->error("Error handling message: " + std::string(e.what()));
        
        // Send error response if appropriate
        if (message.type == "function_request") {
            AgentData error_response;
            error_response.set("success", false);
            error_response.set("error_message", "Internal error: " + std::string(e.what()));
            send_message(message.from_agent, "function_response", error_response);
        }
    }
}

std::string AgentCore::execute_function_async(const std::string& name, const AgentData& params, int priority) {
    if (!running.load()) {
        throw std::runtime_error("Cannot execute function - agent is not running");
    }

    if (name.empty()) {
        throw std::runtime_error("Function name cannot be empty");
    }

    if (priority < 0) {
        throw std::runtime_error("Priority cannot be negative");
    }

    logger->debug("Submitting async function '" + name + "' on agent " + agent_name);

    try {
        return job_manager->submit_job(name, params, priority, agent_id);
    } catch (const std::exception& e) {
        logger->error("Failed to submit async function '" + name + "': " + e.what());
        throw;
    }
}
} // namespace kolosal::agents
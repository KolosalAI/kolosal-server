#pragma once

#include "../export.hpp"
#include "multi_agent_system.hpp"
#include "agent_interfaces.hpp"
#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

namespace kolosal::agents {

/**
 * @brief Advanced orchestration capabilities for multi-agent systems
 */
class KOLOSAL_SERVER_API AgentOrchestrator {
public:
    // Workflow step definition
    struct WorkflowStep {
        std::string agent_id;
        std::string function_name;
        AgentData parameters;
        std::vector<std::string> dependencies; // Step IDs this step depends on
        std::string step_id;
        bool parallel_allowed = true;
        int timeout_seconds = 30;
        int retry_count = 0;
        int max_retries = 3;
    };

    // Workflow definition
    struct Workflow {
        std::string workflow_id;
        std::string name;
        std::string description;
        std::vector<WorkflowStep> steps;
        std::map<std::string, AgentData> global_context;
        bool auto_cleanup = true;
        int max_execution_time_seconds = 300;
    };

    // Workflow execution result
    struct WorkflowResult {
        std::string workflow_id;
        bool success = false;
        std::string error_message;
        std::map<std::string, FunctionResult> step_results;
        double total_execution_time_ms = 0.0;
        std::chrono::time_point<std::chrono::system_clock> start_time;
        std::chrono::time_point<std::chrono::system_clock> end_time;
    };

    // Agent collaboration pattern
    enum class CollaborationPattern {
        SEQUENTIAL,      // Execute agents one after another
        PARALLEL,        // Execute agents in parallel
        PIPELINE,        // Output of one agent becomes input of next
        CONSENSUS,       // Multiple agents vote on result
        HIERARCHY,       // Master-slave pattern
        NEGOTIATION      // Agents negotiate to reach agreement
    };

    // Collaboration group
    struct CollaborationGroup {
        std::string group_id;
        std::string name;
        CollaborationPattern pattern;
        std::vector<std::string> agent_ids;
        std::map<std::string, AgentData> shared_context;
        std::function<AgentData(const std::vector<FunctionResult>&)> result_aggregator;
        int consensus_threshold = 2; // For consensus pattern
        int max_negotiation_rounds = 5; // For negotiation pattern
    };

private:
    std::shared_ptr<YAMLConfigurableAgentManager> agent_manager;
    std::shared_ptr<Logger> logger;

    // Workflow management
    std::map<std::string, Workflow> workflows;
    std::map<std::string, WorkflowResult> workflow_results;
    std::queue<std::string> workflow_queue;
    std::mutex workflow_mutex;
    std::condition_variable workflow_cv;
    std::atomic<bool> orchestrator_running{false};
    std::thread orchestrator_thread;

    // Collaboration management
    std::map<std::string, CollaborationGroup> collaboration_groups;
    std::mutex collaboration_mutex;

    // Monitoring and metrics
    std::atomic<int> active_workflows{0};
    std::atomic<int> completed_workflows{0};
    std::atomic<int> failed_workflows{0};

public:
    explicit AgentOrchestrator(std::shared_ptr<YAMLConfigurableAgentManager> manager);
    ~AgentOrchestrator();

    // Lifecycle management
    void start();
    void stop();
    bool is_running() const { return orchestrator_running.load(); }

    // Workflow management
    bool register_workflow(const Workflow& workflow);
    bool execute_workflow(const std::string& workflow_id, const AgentData& input_context = AgentData());
    bool execute_workflow_async(const std::string& workflow_id, const AgentData& input_context = AgentData());
    WorkflowResult get_workflow_result(const std::string& workflow_id);
    bool cancel_workflow(const std::string& workflow_id);
    std::vector<std::string> list_workflows() const;
    bool remove_workflow(const std::string& workflow_id);

    // Collaboration management
    bool create_collaboration_group(const CollaborationGroup& group);
    bool execute_collaboration(const std::string& group_id, const std::string& task_description, const AgentData& input_data);
    AgentData get_collaboration_result(const std::string& group_id);
    bool remove_collaboration_group(const std::string& group_id);
    std::vector<std::string> list_collaboration_groups() const;

    // Advanced agent coordination
    bool coordinate_agents(const std::vector<std::string>& agent_ids, const std::string& coordination_type, const AgentData& parameters);
    bool setup_agent_pipeline(const std::vector<std::string>& agent_ids, const std::string& pipeline_name);
    bool execute_pipeline(const std::string& pipeline_name, const AgentData& input_data);

    // Monitoring and metrics
    std::map<std::string, int> get_orchestration_metrics() const;
    std::vector<std::string> get_active_workflows() const;
    std::string get_orchestrator_status() const;

    // Load balancing and optimization
    std::string select_optimal_agent(const std::string& capability, const AgentData& context = AgentData());
    bool distribute_workload(const std::string& task_type, const std::vector<AgentData>& tasks);
    void optimize_agent_allocation();

private:
    // Internal workflow execution
    void orchestrator_worker();
    WorkflowResult execute_workflow_internal(const Workflow& workflow, const AgentData& input_context);
    bool execute_workflow_step(const WorkflowStep& step, const AgentData& context, FunctionResult& result);
    bool check_step_dependencies(const WorkflowStep& step, const std::map<std::string, FunctionResult>& completed_steps);
    AgentData merge_context(const AgentData& global_context, const AgentData& step_context);

    // Collaboration execution
    AgentData execute_sequential_collaboration(const CollaborationGroup& group, const AgentData& input_data);
    AgentData execute_parallel_collaboration(const CollaborationGroup& group, const AgentData& input_data);
    AgentData execute_pipeline_collaboration(const CollaborationGroup& group, const AgentData& input_data);
    AgentData execute_consensus_collaboration(const CollaborationGroup& group, const AgentData& input_data);
    AgentData execute_hierarchy_collaboration(const CollaborationGroup& group, const AgentData& input_data);
    AgentData execute_negotiation_collaboration(const CollaborationGroup& group, const AgentData& input_data);

    // Utility methods
    std::string generate_workflow_id();
    std::string generate_group_id();
    double calculate_agent_load(const std::string& agent_id);
    std::vector<std::string> get_agents_by_capability(const std::string& capability);
};

} // namespace kolosal::agents

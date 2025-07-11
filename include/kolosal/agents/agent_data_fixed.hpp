// File: kolosal/agents/agent_data.hpp
#ifndef KOLOSAL_AGENTS_AGENT_DATA_HPP
#define KOLOSAL_AGENTS_AGENT_DATA_HPP

#include <string>
#include <map>
#include <memory>
#include <vector>

namespace kolosal {
namespace agents {

class AgentData {
public:
    AgentData() = default;
    ~AgentData() = default;

    void set_string(const std::string& key, const std::string& value) {
        string_data_[key] = value;
    }

    std::string get_string(const std::string& key) const {
        auto it = string_data_.find(key);
        return (it != string_data_.end()) ? it->second : "";
    }

    bool empty() const {
        return string_data_.empty();
    }

    void clear() {
        string_data_.clear();
    }

private:
    std::map<std::string, std::string> string_data_;
};

class Agent {
public:
    Agent(const std::string& id, const std::string& name, const std::string& type)
        : agent_id_(id), agent_name_(name), agent_type_(type), running_(false) {}

    virtual ~Agent() = default;

    std::string get_agent_id() const { return agent_id_; }
    std::string get_agent_name() const { return agent_name_; }
    std::string get_agent_type() const { return agent_type_; }
    bool is_running() const { return running_; }

    void set_running(bool running) { running_ = running; }

    std::vector<std::string> get_capabilities() const {
        return {"text_processing", "data_analysis", "task_execution"};
    }

private:
    std::string agent_id_;
    std::string agent_name_;
    std::string agent_type_;
    bool running_;
};

struct CommandResult {
    bool success;
    std::string message;
    std::string data;
    std::string error_message;
    long total_execution_time_ms = 0;
    std::map<std::string, CommandResult> step_results;
};

struct WorkflowResult {
    std::string workflow_id;
    bool success = false;
    std::string error_message;
    std::string final_output;
    std::vector<std::string> step_outputs;
    long total_execution_time_ms = 0;
    std::map<std::string, CommandResult> step_results;
};

struct CollaborationGroup {
    std::string group_id;
    std::vector<std::string> agent_ids;
    std::string pattern_type;
};

struct WorkflowMetrics {
    int active_workflows = 0;
    int completed_workflows = 0;
    int failed_workflows = 0;
};

} // namespace agents
} // namespace kolosal

#endif // KOLOSAL_AGENTS_AGENT_DATA_HPP

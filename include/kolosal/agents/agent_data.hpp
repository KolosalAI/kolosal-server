// File: kolosal/agents/agent_data.hpp
#ifndef KOLOSAL_AGENTS_AGENT_DATA_HPP
#define KOLOSAL_AGENTS_AGENT_DATA_HPP

#include <string>
#include <map>
#include <memory>
#include <vector>
#include "json.hpp"

namespace kolosal {
namespace agents {

struct AgentDataValue {
    enum Type {
        NONE,
        STRING,
        INT,
        DOUBLE,
        BOOL,
        ARRAY_STRING,
        OBJECT_DATA
    };

    Type type = NONE;
    std::string s_val;
    int i_val = 0;
    double d_val = 0.0;
    bool b_val = false;
    std::vector<std::string> arr_s_val;
    std::unique_ptr<std::map<std::string, AgentDataValue>> obj_val;

    AgentDataValue() = default;
    AgentDataValue(const std::string& val);
    AgentDataValue(int val);
    AgentDataValue(double val);
    AgentDataValue(bool val);
    AgentDataValue(const std::vector<std::string>& val);
    AgentDataValue(const class AgentData& val);
    AgentDataValue(const AgentDataValue& other);
    AgentDataValue& operator=(const AgentDataValue& other);
};

class AgentData {
public:
    AgentData() = default;
    ~AgentData() = default;

    // Set methods for different types
    void set(const std::string& key, const std::string& value) {
        data[key] = AgentDataValue(value);
    }
    
    void set(const std::string& key, int value) {
        data[key] = AgentDataValue(value);
    }
    
    void set(const std::string& key, double value) {
        data[key] = AgentDataValue(value);
    }
    
    void set(const std::string& key, bool value) {
        data[key] = AgentDataValue(value);
    }
    
    void set(const std::string& key, const std::vector<std::string>& value) {
        data[key] = AgentDataValue(value);
    }
    
    void set(const std::string& key, const AgentData& value) {
        data[key] = AgentDataValue(value);
    }
    
    void set(const std::string& key, const AgentDataValue& value) {
        data[key] = value;
    }

    // Get methods with defaults
    std::string get_string(const std::string& key, const std::string& default_val = "") const;
    int get_int(const std::string& key, int default_val = 0) const;
    double get_double(const std::string& key, double default_val = 0.0) const;
    bool get_bool(const std::string& key, bool default_val = false) const;
    std::vector<std::string> get_array_string(const std::string& key) const;

    // Utility methods
    bool has_key(const std::string& key) const;
    void clear();
    std::vector<std::string> get_all_keys() const;
    std::vector<std::string> get_keys() const { return get_all_keys(); }
    std::string to_string() const;
    
    // Get the underlying data
    const std::map<std::string, AgentDataValue>& get_data() const { return data; }

    // JSON conversion
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& json_data);

private:
    std::map<std::string, AgentDataValue> data;
};

class UUIDGenerator {
public:
    static std::string generate();
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

} // namespace agents
} // namespace kolosal

#endif // KOLOSAL_AGENTS_AGENT_DATA_HPP

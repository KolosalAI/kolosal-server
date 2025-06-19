// File: include/kolosal/agents/agent_data.hpp
#pragma once

#include "../export.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace kolosal::agents {

class AgentData;

/**
 * @brief Flexible data container for agent parameters and results
 */
struct KOLOSAL_SERVER_API AgentDataValue {
    enum Type { NONE, STRING, INT, DOUBLE, BOOL, ARRAY_STRING, OBJECT_DATA };

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
    AgentDataValue(const AgentData& val);

    // Copy constructor and assignment operator
    AgentDataValue(const AgentDataValue& other);
    AgentDataValue& operator=(const AgentDataValue& other);
    
    ~AgentDataValue() = default;
};

/**
 * @brief Key-value data container for agent communication
 */
class KOLOSAL_SERVER_API AgentData {
private:
    std::map<std::string, AgentDataValue> data;

public:
    template<typename T>
    void set(const std::string& key, const T& value) {
        data[key] = AgentDataValue(value);
    }

    std::string get_string(const std::string& key, const std::string& default_val = "") const;
    int get_int(const std::string& key, int default_val = 0) const;
    double get_double(const std::string& key, double default_val = 0.0) const;
    bool get_bool(const std::string& key, bool default_val = false) const;
    std::vector<std::string> get_array_string(const std::string& key) const;

    bool has_key(const std::string& key) const;
    void clear();
    std::vector<std::string> get_all_keys() const;
    const std::map<std::string, AgentDataValue>& get_data() const { return data; }
};

/**
 * @brief UUID Generator for unique identifiers
 */
class KOLOSAL_SERVER_API UUIDGenerator {
public:
    static std::string generate();
};

} // namespace kolosal::agents
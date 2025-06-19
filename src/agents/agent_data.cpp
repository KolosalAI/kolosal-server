// File: src/agents/agent_data.cpp
#include "kolosal/agents/agent_data.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <chrono>

namespace kolosal::agents {

// AgentDataValue implementation
AgentDataValue::AgentDataValue(const AgentDataValue& other) : type(other.type) {
    switch (type) {
    case STRING: s_val = other.s_val; break;
    case INT: i_val = other.i_val; break;
    case DOUBLE: d_val = other.d_val; break;
    case BOOL: b_val = other.b_val; break;
    case ARRAY_STRING: arr_s_val = other.arr_s_val; break;
    case OBJECT_DATA:
        if (other.obj_val) {
            obj_val = std::make_unique<std::map<std::string, AgentDataValue>>(*other.obj_val);
        }
        break;
    case NONE: break;
    }
}

AgentDataValue::AgentDataValue(const AgentData& val) : type(OBJECT_DATA) {
    obj_val = std::make_unique<std::map<std::string, AgentDataValue>>(val.get_data());
}

AgentDataValue& AgentDataValue::operator=(const AgentDataValue& other) {
    if (this != &other) {
        type = other.type;
        switch (type) {
        case STRING: s_val = other.s_val; break;
        case INT: i_val = other.i_val; break;
        case DOUBLE: d_val = other.d_val; break;
        case BOOL: b_val = other.b_val; break;
        case ARRAY_STRING: arr_s_val = other.arr_s_val; break;
        case OBJECT_DATA:
            if (other.obj_val) {
                obj_val = std::make_unique<std::map<std::string, AgentDataValue>>(*other.obj_val);
            } else {
                obj_val.reset();
            }
            break;
        case NONE: 
            obj_val.reset();
            break;
        }
    }
    return *this;
}

// AgentDataValue missing constructors
AgentDataValue::AgentDataValue(const std::string& val) : type(STRING), s_val(val) {}
AgentDataValue::AgentDataValue(int val) : type(INT), i_val(val) {}
AgentDataValue::AgentDataValue(double val) : type(DOUBLE), d_val(val) {}
AgentDataValue::AgentDataValue(bool val) : type(BOOL), b_val(val) {}
AgentDataValue::AgentDataValue(const std::vector<std::string>& val) : type(ARRAY_STRING), arr_s_val(val) {}

// AgentData implementation
std::string AgentData::get_string(const std::string& key, const std::string& default_val) const {
    auto it = data.find(key);
    if (it != data.end() && it->second.type == AgentDataValue::STRING) {
        return it->second.s_val;
    }
    return default_val;
}

int AgentData::get_int(const std::string& key, int default_val) const {
    auto it = data.find(key);
    if (it != data.end() && it->second.type == AgentDataValue::INT) {
        return it->second.i_val;
    }
    return default_val;
}

double AgentData::get_double(const std::string& key, double default_val) const {
    auto it = data.find(key);
    if (it != data.end() && it->second.type == AgentDataValue::DOUBLE) {
        return it->second.d_val;
    }
    return default_val;
}

bool AgentData::get_bool(const std::string& key, bool default_val) const {
    auto it = data.find(key);
    if (it != data.end() && it->second.type == AgentDataValue::BOOL) {
        return it->second.b_val;
    }
    return default_val;
}

std::vector<std::string> AgentData::get_array_string(const std::string& key) const {
    auto it = data.find(key);
    if (it != data.end() && it->second.type == AgentDataValue::ARRAY_STRING) {
        return it->second.arr_s_val;
    }
    return {};
}

bool AgentData::has_key(const std::string& key) const {
    return data.count(key) > 0;
}

void AgentData::clear() {
    data.clear();
}

std::vector<std::string> AgentData::get_all_keys() const {
    std::vector<std::string> keys;
    for (const auto& pair : data) {
        keys.push_back(pair.first);
    }
    return keys;
}

// UUIDGenerator implementation - static members defined here
namespace {
    std::mutex& get_uuid_mutex() {
        static std::mutex uuid_mutex;
        return uuid_mutex;
    }
    
    std::mt19937& get_uuid_generator() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return gen;
    }
    
    std::uniform_int_distribution<>& get_uuid_distribution() {
        static std::uniform_int_distribution<> dis(0, 15);
        return dis;
    }
}

std::string UUIDGenerator::generate() {
    std::lock_guard<std::mutex> lock(get_uuid_mutex());
    
    auto& gen = get_uuid_generator();
    auto& dis = get_uuid_distribution();
    
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 8; ++i) oss << dis(gen);
    oss << "-";
    for (int i = 0; i < 4; ++i) oss << dis(gen);
    oss << "-4";
    for (int i = 0; i < 3; ++i) oss << dis(gen);
    oss << "-";
    oss << (8 + (dis(gen) % 4));
    for (int i = 0; i < 3; ++i) oss << dis(gen);
    oss << "-";
    for (int i = 0; i < 12; ++i) oss << dis(gen);
    return oss.str();
}

} // namespace kolosal::agents
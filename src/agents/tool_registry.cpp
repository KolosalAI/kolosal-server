// File: src/agents/tool_registry.cpp
#include "kolosal/agents/tool_registry.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

namespace kolosal::agents {

// Bridge Logger interface to ServerLogger
class Logger {
public:
    void info(const std::string& message) { ServerLogger::logInfo("%s", message.c_str()); }
    void debug(const std::string& message) { ServerLogger::logDebug("%s", message.c_str()); }
    void warn(const std::string& message) { ServerLogger::logWarning("%s", message.c_str()); }
    void error(const std::string& message) { ServerLogger::logError("%s", message.c_str()); }
};

std::string ToolSchema::to_json_schema() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"name\": \"" << name << "\",\n";
    json << "  \"description\": \"" << description << "\",\n";
    json << "  \"category\": \"" << category << "\",\n";
    json << "  \"parameters\": {\n";
    json << "    \"type\": \"object\",\n";
    json << "    \"properties\": {\n";
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        const auto& param = parameters[i];
        json << "      \"" << param.name << "\": {\n";
        json << "        \"type\": \"" << param.type << "\",\n";
        json << "        \"description\": \"" << param.description << "\"";
        
        if (!param.default_value.empty()) {
            json << ",\n        \"default\": \"" << param.default_value << "\"";
        }
        
        if (!param.enum_values.empty()) {
            json << ",\n        \"enum\": [";
            for (size_t j = 0; j < param.enum_values.size(); ++j) {
                json << "\"" << param.enum_values[j] << "\"";
                if (j < param.enum_values.size() - 1) json << ", ";
            }
            json << "]";
        }
        
        json << "\n      }";
        if (i < parameters.size() - 1) json << ",";
        json << "\n";
    }
    
    json << "    },\n";
    json << "    \"required\": [";
    
    bool first_required = true;
    for (const auto& param : parameters) {
        if (param.required) {
            if (!first_required) json << ", ";
            json << "\"" << param.name << "\"";
            first_required = false;
        }
    }
    
    json << "]\n";
    json << "  }\n";
    json << "}";
    
    return json.str();
}

bool Tool::validate_parameters(const AgentData& params) const {
    const auto schema = get_schema();
    
    // Check required parameters
    for (const auto& param : schema.parameters) {
        if (param.required) {
            if (param.type == "string" && params.get_string(param.name).empty()) {
                return false;
            } else if (param.type == "number" && !params.has_key(param.name)) {
                return false;
            } else if (param.type == "boolean" && !params.has_key(param.name)) {
                return false;
            }
        }
    }
    
    return true;
}

bool ToolFilter::matches(const Tool* tool, const AgentData& params) const {
    if (!tool) return false;
    
    // Check categories
    if (!categories.empty()) {
        bool category_match = false;
        for (const auto& cat : categories) {
            if (tool->get_category() == cat) {
                category_match = true;
                break;
            }
        }
        if (!category_match) return false;
    }
    
    // Check tags
    if (!tags.empty()) {
        bool tag_match = false;
        const auto tool_tags = tool->get_tags();
        for (const auto& tag : tags) {
            if (std::find(tool_tags.begin(), tool_tags.end(), tag) != tool_tags.end()) {
                tag_match = true;
                break;
            }
        }
        if (!tag_match) return false;
    }
    
    // Check name pattern
    if (!name_pattern.empty()) {
        try {
            std::regex pattern(name_pattern, std::regex_constants::icase);
            if (!std::regex_search(tool->get_name(), pattern)) {
                return false;
            }
        } catch (const std::regex_error&) {
            // If regex is invalid, do simple string contains check
            std::string tool_name_lower = tool->get_name();
            std::string pattern_lower = name_pattern;
            std::transform(tool_name_lower.begin(), tool_name_lower.end(), tool_name_lower.begin(), ::tolower);
            std::transform(pattern_lower.begin(), pattern_lower.end(), pattern_lower.begin(), ::tolower);
            if (tool_name_lower.find(pattern_lower) == std::string::npos) {
                return false;
            }
        }
    }
    
    // Check cost
    if (max_cost >= 0.0) {
        double estimated_cost = tool->estimate_cost(params);
        if (estimated_cost > max_cost) {
            return false;
        }
    }
    
    return true;
}

ToolRegistry::ToolRegistry(std::shared_ptr<Logger> log) : logger(log) {
    if (!logger) {
        logger = std::make_shared<Logger>();
    }
}

bool ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    if (!tool) return false;
    
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    const std::string name = tool->get_name();
    
    if (tools.find(name) != tools.end()) {
        logger->warn("Tool already registered: " + name);
        return false;
    }
    
    update_indices(name, tool.get());
    tools[name] = std::move(tool);
    
    logger->info("Registered tool: " + name);
    return true;
}

bool ToolRegistry::unregister_tool(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    auto it = tools.find(name);
    if (it == tools.end()) {
        return false;
    }
    
    remove_from_indices(name, it->second.get());
    tools.erase(it);
    
    logger->info("Unregistered tool: " + name);
    return true;
}

std::vector<std::string> ToolRegistry::discover_tools(const ToolFilter& filter) const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    std::vector<std::string> result;
    
    for (const auto& pair : tools) {
        if (filter.matches(pair.second.get())) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

std::vector<ToolSchema> ToolRegistry::get_tool_schemas(const ToolFilter& filter) const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    std::vector<ToolSchema> schemas;
    
    for (const auto& pair : tools) {
        if (filter.matches(pair.second.get())) {
            schemas.push_back(pair.second->get_schema());
        }
    }
    
    return schemas;
}

Tool* ToolRegistry::get_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    auto it = tools.find(name);
    return (it != tools.end()) ? it->second.get() : nullptr;
}

ToolSchema ToolRegistry::get_tool_schema(const std::string& name) const {
    Tool* tool = get_tool(name);
    if (tool) {
        return tool->get_schema();
    }
    throw std::runtime_error("Tool not found: " + name);
}

bool ToolRegistry::has_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    return tools.find(name) != tools.end();
}

std::vector<std::string> ToolRegistry::get_categories() const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    std::vector<std::string> categories;
    for (const auto& pair : category_index) {
        categories.push_back(pair.first);
    }
    return categories;
}

std::vector<std::string> ToolRegistry::get_tags() const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    std::vector<std::string> tags;
    for (const auto& pair : tag_index) {
        tags.push_back(pair.first);
    }
    return tags;
}

std::vector<std::string> ToolRegistry::get_tools_by_category(const std::string& category) const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    auto it = category_index.find(category);
    return (it != category_index.end()) ? it->second : std::vector<std::string>();
}

std::vector<std::string> ToolRegistry::get_tools_by_tag(const std::string& tag) const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    auto it = tag_index.find(tag);
    return (it != tag_index.end()) ? it->second : std::vector<std::string>();
}

FunctionResult ToolRegistry::execute_tool(const std::string& name, const AgentData& params, 
                                         const ToolContext& context) const {
    Tool* tool = get_tool(name);
    if (!tool) {
        return FunctionResult(false, "Tool not found: " + name);
    }
    
    // Validate parameters
    if (!tool->validate_parameters(params)) {
        return FunctionResult(false, "Invalid parameters for tool: " + name);
    }
    
    try {
        return tool->execute(params, context);
    } catch (const std::exception& e) {
        return FunctionResult(false, "Tool execution error: " + std::string(e.what()));
    }
}

size_t ToolRegistry::get_tool_count() const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    return tools.size();
}

std::unordered_map<std::string, size_t> ToolRegistry::get_category_stats() const {
    std::lock_guard<std::mutex> lock(registry_mutex);
    
    std::unordered_map<std::string, size_t> stats;
    for (const auto& pair : category_index) {
        stats[pair.first] = pair.second.size();
    }
    return stats;
}

void ToolRegistry::update_indices(const std::string& tool_name, const Tool* tool) {
    // Update category index
    const std::string category = tool->get_category();
    category_index[category].push_back(tool_name);
    
    // Update tag index
    const auto tags = tool->get_tags();
    for (const auto& tag : tags) {
        tag_index[tag].push_back(tool_name);
    }
}

void ToolRegistry::remove_from_indices(const std::string& tool_name, const Tool* tool) {
    // Remove from category index
    const std::string category = tool->get_category();
    auto& cat_tools = category_index[category];
    cat_tools.erase(std::remove(cat_tools.begin(), cat_tools.end(), tool_name), cat_tools.end());
    if (cat_tools.empty()) {
        category_index.erase(category);
    }
    
    // Remove from tag index
    const auto tags = tool->get_tags();
    for (const auto& tag : tags) {
        auto& tag_tools = tag_index[tag];
        tag_tools.erase(std::remove(tag_tools.begin(), tag_tools.end(), tool_name), tag_tools.end());
        if (tag_tools.empty()) {
            tag_index.erase(tag);
        }
    }
}

// BaseTool implementation
BaseTool::BaseTool(const std::string& tool_name, const std::string& desc, const std::string& cat)
    : name(tool_name), description(desc), category(cat), schema(tool_name, desc, cat) {
}

BaseTool& BaseTool::add_parameter(const ToolParameter& param) {
    schema.parameters.push_back(param);
    return *this;
}

BaseTool& BaseTool::add_tag(const std::string& tag) {
    tags.push_back(tag);
    return *this;
}

BaseTool& BaseTool::set_category(const std::string& cat) {
    category = cat;
    schema.category = cat;
    return *this;
}

} // namespace kolosal::agents

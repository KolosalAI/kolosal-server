// File: include/kolosal/agents/tool_registry.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include "agent_data.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>

namespace kolosal::agents {

/**
 * @brief Tool parameter definition
 */
struct KOLOSAL_SERVER_API ToolParameter {
    std::string name;
    std::string type;  // "string", "number", "boolean", "object", "array"
    std::string description;
    bool required = false;
    std::string default_value;
    std::vector<std::string> enum_values;  // For enum types
    
    ToolParameter(const std::string& param_name, const std::string& param_type, 
                 const std::string& desc, bool is_required = false)
        : name(param_name), type(param_type), description(desc), required(is_required) {}
};

/**
 * @brief Tool schema definition (JSON Schema-like)
 */
struct KOLOSAL_SERVER_API ToolSchema {
    std::string name;
    std::string description;
    std::string category;
    std::vector<ToolParameter> parameters;
    std::vector<std::string> required_capabilities;
    std::vector<std::string> tags;
    
    ToolSchema(const std::string& tool_name, const std::string& desc, 
              const std::string& cat = "general")
        : name(tool_name), description(desc), category(cat) {}
        
    // Helper to add parameters
    ToolSchema& add_parameter(const ToolParameter& param) {
        parameters.push_back(param);
        return *this;
    }
    
    // Generate JSON Schema representation
    std::string to_json_schema() const;
};

/**
 * @brief Tool execution context
 */
struct KOLOSAL_SERVER_API ToolContext {
    std::string agent_id;
    std::string execution_id;
    std::unordered_map<std::string, std::string> environment;
    std::shared_ptr<class Logger> logger;
    
    ToolContext(const std::string& aid) : agent_id(aid) {}
};

/**
 * @brief Enhanced tool interface with schema support
 */
class KOLOSAL_SERVER_API Tool {
public:
    virtual ~Tool() = default;
    
    // Basic tool information
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
    virtual std::string get_category() const { return "general"; }
    virtual std::vector<std::string> get_tags() const { return {}; }
    
    // Schema definition
    virtual ToolSchema get_schema() const = 0;
    
    // Execution
    virtual FunctionResult execute(const AgentData& params, const ToolContext& context) = 0;
    
    // Validation
    virtual bool validate_parameters(const AgentData& params) const;
    
    // Cost estimation (for expensive operations)
    virtual double estimate_cost(const AgentData& params) const { return 0.0; }
    
    // Async support
    virtual bool supports_async() const { return false; }
};

/**
 * @brief Tool discovery and filtering
 */
struct KOLOSAL_SERVER_API ToolFilter {
    std::vector<std::string> categories;
    std::vector<std::string> tags;
    std::vector<std::string> required_capabilities;
    std::string name_pattern;
    double max_cost = -1.0;  // -1 means no limit
    
    bool matches(const Tool* tool, const AgentData& params = AgentData()) const;
};

/**
 * @brief Comprehensive tool registry with discovery and management
 */
class KOLOSAL_SERVER_API ToolRegistry {
private:
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools;
    std::unordered_map<std::string, std::vector<std::string>> category_index;
    std::unordered_map<std::string, std::vector<std::string>> tag_index;
    mutable std::mutex registry_mutex;
    std::shared_ptr<class Logger> logger;
    
public:
    ToolRegistry(std::shared_ptr<class Logger> log = nullptr);
    
    // Tool management
    bool register_tool(std::unique_ptr<Tool> tool);
    bool unregister_tool(const std::string& name);
    
    // Tool discovery
    std::vector<std::string> discover_tools(const ToolFilter& filter = ToolFilter()) const;
    std::vector<ToolSchema> get_tool_schemas(const ToolFilter& filter = ToolFilter()) const;
    
    // Tool information
    Tool* get_tool(const std::string& name) const;
    ToolSchema get_tool_schema(const std::string& name) const;
    bool has_tool(const std::string& name) const;
    
    // Categories and tags
    std::vector<std::string> get_categories() const;
    std::vector<std::string> get_tags() const;
    std::vector<std::string> get_tools_by_category(const std::string& category) const;
    std::vector<std::string> get_tools_by_tag(const std::string& tag) const;
    
    // Execution
    FunctionResult execute_tool(const std::string& name, const AgentData& params, 
                               const ToolContext& context) const;
    
    // Registry statistics
    size_t get_tool_count() const;
    std::unordered_map<std::string, size_t> get_category_stats() const;
    
private:
    void update_indices(const std::string& tool_name, const Tool* tool);
    void remove_from_indices(const std::string& tool_name, const Tool* tool);
};

/**
 * @brief Helper base class for implementing tools
 */
class KOLOSAL_SERVER_API BaseTool : public Tool {
protected:
    std::string name;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
    ToolSchema schema;
    
public:
    BaseTool(const std::string& tool_name, const std::string& desc, 
            const std::string& cat = "general");
    
    // Tool interface implementation
    std::string get_name() const override { return name; }
    std::string get_description() const override { return description; }
    std::string get_category() const override { return category; }
    std::vector<std::string> get_tags() const override { return tags; }
    ToolSchema get_schema() const override { return schema; }
    
    // Helper methods for subclasses
    BaseTool& add_parameter(const ToolParameter& param);
    BaseTool& add_tag(const std::string& tag);
    BaseTool& set_category(const std::string& cat);
};

} // namespace kolosal::agents

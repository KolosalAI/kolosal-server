// File: include/kolosal/agents/agent_roles.hpp
#pragma once

#include "../export.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace kolosal::agents {

/**
 * @brief Predefined agent roles with specific capabilities
 */
enum class KOLOSAL_SERVER_API AgentRole {
    GENERIC = 0,
    RESEARCHER,
    ANALYST,
    WRITER,
    CRITIC,
    EXECUTOR,
    COORDINATOR,
    SPECIALIST,
    ASSISTANT
};

/**
 * @brief Agent specialization areas
 */
enum class KOLOSAL_SERVER_API AgentSpecialization {
    NONE = 0,
    DATA_ANALYSIS,
    TEXT_PROCESSING,
    CODE_GENERATION,
    DOCUMENT_ANALYSIS,
    WEB_RESEARCH,
    REASONING,
    PLANNING,
    EXECUTION
};

/**
 * @brief Capability levels for agents
 */
enum class KOLOSAL_SERVER_API CapabilityLevel {
    BASIC = 1,
    INTERMEDIATE = 2,
    ADVANCED = 3,
    EXPERT = 4
};

/**
 * @brief Agent capability definition
 */
struct KOLOSAL_SERVER_API AgentCapability {
    std::string name;
    std::string description;
    CapabilityLevel level;
    std::vector<std::string> required_functions;
    std::vector<std::string> dependencies;

    AgentCapability(const std::string& cap_name, const std::string& desc, 
                   CapabilityLevel cap_level = CapabilityLevel::BASIC)
        : name(cap_name), description(desc), level(cap_level) {}
};

/**
 * @brief Agent role definition with predefined capabilities
 */
struct KOLOSAL_SERVER_API AgentRoleDefinition {
    AgentRole role;
    std::string name;
    std::string description;
    std::vector<AgentCapability> capabilities;
    std::vector<AgentSpecialization> specializations;
    std::vector<std::string> default_functions;

    // Default constructor
    AgentRoleDefinition() : role(AgentRole::GENERIC) {}
    
    AgentRoleDefinition(AgentRole agent_role, const std::string& role_name, 
                       const std::string& desc)
        : role(agent_role), name(role_name), description(desc) {}
};

/**
 * @brief Role management and configuration
 */
class KOLOSAL_SERVER_API AgentRoleManager {
private:
    std::unordered_map<AgentRole, AgentRoleDefinition> role_definitions;
    
public:
    AgentRoleManager();
    
    // Role management
    const AgentRoleDefinition& get_role_definition(AgentRole role) const;
    std::vector<AgentRole> get_available_roles() const;
    
    // Capability management
    std::vector<AgentCapability> get_role_capabilities(AgentRole role) const;
    bool has_capability(AgentRole role, const std::string& capability) const;
    
    // Role queries
    std::string role_to_string(AgentRole role) const;
    AgentRole string_to_role(const std::string& role_str) const;
    
    // Specialization helpers
    std::string specialization_to_string(AgentSpecialization spec) const;
    AgentSpecialization string_to_specialization(const std::string& spec_str) const;

private:
    void initialize_default_roles();
};

} // namespace kolosal::agents

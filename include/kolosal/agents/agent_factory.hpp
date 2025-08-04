// File: include/kolosal/agents/agent_factory.hpp
#pragma once

#include "../export.hpp"
#include "agent_core.hpp"
#include "agent_roles.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace kolosal::agents {

/**
 * @brief Configuration for creating specialized agents
 */
struct KOLOSAL_SERVER_API AgentConfiguration {
    std::string name;
    std::string type;
    AgentRole role;
    std::vector<AgentSpecialization> specializations;
    std::vector<std::string> custom_capabilities;
    std::unordered_map<std::string, std::string> metadata;
    
    AgentConfiguration(const std::string& agent_name = "", 
                      AgentRole agent_role = AgentRole::GENERIC)
        : name(agent_name), role(agent_role) {}
};

/**
 * @brief Factory for creating and configuring agents
 */
class KOLOSAL_SERVER_API AgentFactory {
public:
    // Predefined agent creation methods
    static std::shared_ptr<AgentCore> create_researcher_agent(const std::string& name = "");
    static std::shared_ptr<AgentCore> create_analyst_agent(const std::string& name = "");
    static std::shared_ptr<AgentCore> create_writer_agent(const std::string& name = "");
    static std::shared_ptr<AgentCore> create_critic_agent(const std::string& name = "");
    static std::shared_ptr<AgentCore> create_coordinator_agent(const std::string& name = "");
    
    // Generic agent creation
    static std::shared_ptr<AgentCore> create_agent(const AgentConfiguration& config);
    
    // Specialized agent creation
    static std::shared_ptr<AgentCore> create_document_processing_agent(const std::string& name = "");
    static std::shared_ptr<AgentCore> create_web_research_agent(const std::string& name = "");
    static std::shared_ptr<AgentCore> create_code_generation_agent(const std::string& name = "");
    static std::shared_ptr<AgentCore> create_data_analysis_agent(const std::string& name = "");
    
    // Agent team creation
    static std::vector<std::shared_ptr<AgentCore>> create_research_team();
    static std::vector<std::shared_ptr<AgentCore>> create_content_creation_team();
    static std::vector<std::shared_ptr<AgentCore>> create_analysis_team();

private:
    static void configure_agent_for_role(std::shared_ptr<AgentCore> agent, AgentRole role);
    static void add_role_specific_tools(std::shared_ptr<AgentCore> agent, AgentRole role);
};

} // namespace kolosal::agents

// File: src/agents/agent_factory.cpp
#include "kolosal/agents/agent_factory.hpp"
#include "kolosal/agents/tool_registry.hpp"

namespace kolosal::agents {

std::shared_ptr<AgentCore> AgentFactory::create_researcher_agent(const std::string& name) {
    std::string agent_name = name.empty() ? "Researcher" : name;
    auto agent = std::make_shared<AgentCore>(agent_name, "researcher", AgentRole::RESEARCHER);
    
    agent->add_specialization(AgentSpecialization::WEB_RESEARCH);
    agent->add_specialization(AgentSpecialization::DOCUMENT_ANALYSIS);
    
    configure_agent_for_role(agent, AgentRole::RESEARCHER);
    return agent;
}

std::shared_ptr<AgentCore> AgentFactory::create_analyst_agent(const std::string& name) {
    std::string agent_name = name.empty() ? "Analyst" : name;
    auto agent = std::make_shared<AgentCore>(agent_name, "analyst", AgentRole::ANALYST);
    
    agent->add_specialization(AgentSpecialization::DATA_ANALYSIS);
    agent->add_specialization(AgentSpecialization::REASONING);
    
    configure_agent_for_role(agent, AgentRole::ANALYST);
    return agent;
}

std::shared_ptr<AgentCore> AgentFactory::create_writer_agent(const std::string& name) {
    std::string agent_name = name.empty() ? "Writer" : name;
    auto agent = std::make_shared<AgentCore>(agent_name, "writer", AgentRole::WRITER);
    
    agent->add_specialization(AgentSpecialization::TEXT_PROCESSING);
    agent->add_specialization(AgentSpecialization::CODE_GENERATION);
    
    configure_agent_for_role(agent, AgentRole::WRITER);
    return agent;
}

std::shared_ptr<AgentCore> AgentFactory::create_critic_agent(const std::string& name) {
    std::string agent_name = name.empty() ? "Critic" : name;
    auto agent = std::make_shared<AgentCore>(agent_name, "critic", AgentRole::CRITIC);
    
    agent->add_specialization(AgentSpecialization::REASONING);
    
    configure_agent_for_role(agent, AgentRole::CRITIC);
    return agent;
}

std::shared_ptr<AgentCore> AgentFactory::create_coordinator_agent(const std::string& name) {
    std::string agent_name = name.empty() ? "Coordinator" : name;
    auto agent = std::make_shared<AgentCore>(agent_name, "coordinator", AgentRole::COORDINATOR);
    
    agent->add_specialization(AgentSpecialization::PLANNING);
    agent->add_specialization(AgentSpecialization::EXECUTION);
    
    configure_agent_for_role(agent, AgentRole::COORDINATOR);
    return agent;
}

std::shared_ptr<AgentCore> AgentFactory::create_agent(const AgentConfiguration& config) {
    auto agent = std::make_shared<AgentCore>(config.name, config.type, config.role);
    
    // Add specializations
    for (auto spec : config.specializations) {
        agent->add_specialization(spec);
    }
    
    // Add custom capabilities
    for (const auto& capability : config.custom_capabilities) {
        agent->add_capability(capability);
    }
    
    configure_agent_for_role(agent, config.role);
    return agent;
}

std::shared_ptr<AgentCore> AgentFactory::create_document_processing_agent(const std::string& name) {
    AgentConfiguration config(name.empty() ? "DocumentProcessor" : name, AgentRole::SPECIALIST);
    config.specializations = {AgentSpecialization::DOCUMENT_ANALYSIS, AgentSpecialization::TEXT_PROCESSING};
    config.custom_capabilities = {"pdf_parsing", "document_extraction", "content_analysis"};
    
    return create_agent(config);
}

std::shared_ptr<AgentCore> AgentFactory::create_web_research_agent(const std::string& name) {
    AgentConfiguration config(name.empty() ? "WebResearcher" : name, AgentRole::RESEARCHER);
    config.specializations = {AgentSpecialization::WEB_RESEARCH, AgentSpecialization::DATA_ANALYSIS};
    config.custom_capabilities = {"web_scraping", "search_optimization", "source_validation"};
    
    return create_agent(config);
}

std::shared_ptr<AgentCore> AgentFactory::create_code_generation_agent(const std::string& name) {
    AgentConfiguration config(name.empty() ? "CodeGenerator" : name, AgentRole::WRITER);
    config.specializations = {AgentSpecialization::CODE_GENERATION, AgentSpecialization::REASONING};
    config.custom_capabilities = {"code_analysis", "refactoring", "testing", "documentation"};
    
    return create_agent(config);
}

std::shared_ptr<AgentCore> AgentFactory::create_data_analysis_agent(const std::string& name) {
    AgentConfiguration config(name.empty() ? "DataAnalyst" : name, AgentRole::ANALYST);
    config.specializations = {AgentSpecialization::DATA_ANALYSIS, AgentSpecialization::REASONING};
    config.custom_capabilities = {"statistical_analysis", "data_visualization", "pattern_recognition"};
    
    return create_agent(config);
}

std::vector<std::shared_ptr<AgentCore>> AgentFactory::create_research_team() {
    std::vector<std::shared_ptr<AgentCore>> team;
    
    team.push_back(create_coordinator_agent("ResearchCoordinator"));
    team.push_back(create_researcher_agent("PrimaryResearcher"));
    team.push_back(create_web_research_agent("WebSpecialist"));
    team.push_back(create_document_processing_agent("DocumentSpecialist"));
    team.push_back(create_analyst_agent("ResearchAnalyst"));
    
    return team;
}

std::vector<std::shared_ptr<AgentCore>> AgentFactory::create_content_creation_team() {
    std::vector<std::shared_ptr<AgentCore>> team;
    
    team.push_back(create_coordinator_agent("ContentCoordinator"));
    team.push_back(create_researcher_agent("ContentResearcher"));
    team.push_back(create_writer_agent("ContentWriter"));
    team.push_back(create_critic_agent("ContentCritic"));
    team.push_back(create_analyst_agent("ContentAnalyst"));
    
    return team;
}

std::vector<std::shared_ptr<AgentCore>> AgentFactory::create_analysis_team() {
    std::vector<std::shared_ptr<AgentCore>> team;
    
    team.push_back(create_coordinator_agent("AnalysisCoordinator"));
    team.push_back(create_data_analysis_agent("PrimaryAnalyst"));
    team.push_back(create_analyst_agent("SecondaryAnalyst"));
    team.push_back(create_critic_agent("AnalysisCritic"));
    
    return team;
}

void AgentFactory::configure_agent_for_role(std::shared_ptr<AgentCore> agent, AgentRole role) {
    // Add role-specific tools and configurations
    add_role_specific_tools(agent, role);
    
    // Configure memory settings based on role
    if (auto memory_manager = agent->get_memory_manager()) {
        // Different roles might have different memory requirements
        switch (role) {
            case AgentRole::RESEARCHER:
                // Researchers need more long-term memory
                break;
            case AgentRole::ANALYST:
                // Analysts need working memory for calculations
                break;
            case AgentRole::WRITER:
                // Writers need conversation memory for context
                break;
            default:
                break;
        }
    }
}

void AgentFactory::add_role_specific_tools(std::shared_ptr<AgentCore> agent, AgentRole role) {
    auto tool_registry = agent->get_tool_registry();
    if (!tool_registry) return;
    
    // Add role-specific custom tools here
    // This is where you would register specialized tools for each role
    
    switch (role) {
        case AgentRole::RESEARCHER:
            // Add research-specific tools
            break;
        case AgentRole::ANALYST:
            // Add analysis-specific tools
            break;
        case AgentRole::WRITER:
            // Add writing-specific tools
            break;
        case AgentRole::CRITIC:
            // Add evaluation-specific tools
            break;
        default:
            break;
    }
}

} // namespace kolosal::agents

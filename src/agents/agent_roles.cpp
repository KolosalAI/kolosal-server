// File: src/agents/agent_roles.cpp
#include "kolosal/agents/agent_roles.hpp"
#include <stdexcept>

namespace kolosal::agents {

AgentRoleManager::AgentRoleManager() {
    initialize_default_roles();
}

const AgentRoleDefinition& AgentRoleManager::get_role_definition(AgentRole role) const {
    auto it = role_definitions.find(role);
    if (it == role_definitions.end()) {
        throw std::runtime_error("Role definition not found");
    }
    return it->second;
}

std::vector<AgentRole> AgentRoleManager::get_available_roles() const {
    std::vector<AgentRole> roles;
    for (const auto& pair : role_definitions) {
        roles.push_back(pair.first);
    }
    return roles;
}

std::vector<AgentCapability> AgentRoleManager::get_role_capabilities(AgentRole role) const {
    return get_role_definition(role).capabilities;
}

bool AgentRoleManager::has_capability(AgentRole role, const std::string& capability) const {
    const auto& definition = get_role_definition(role);
    for (const auto& cap : definition.capabilities) {
        if (cap.name == capability) {
            return true;
        }
    }
    return false;
}

std::string AgentRoleManager::role_to_string(AgentRole role) const {
    switch (role) {
        case AgentRole::GENERIC: return "generic";
        case AgentRole::RESEARCHER: return "researcher";
        case AgentRole::ANALYST: return "analyst";
        case AgentRole::WRITER: return "writer";
        case AgentRole::CRITIC: return "critic";
        case AgentRole::EXECUTOR: return "executor";
        case AgentRole::COORDINATOR: return "coordinator";
        case AgentRole::SPECIALIST: return "specialist";
        case AgentRole::ASSISTANT: return "assistant";
        default: return "unknown";
    }
}

AgentRole AgentRoleManager::string_to_role(const std::string& role_str) const {
    if (role_str == "generic") return AgentRole::GENERIC;
    if (role_str == "researcher") return AgentRole::RESEARCHER;
    if (role_str == "analyst") return AgentRole::ANALYST;
    if (role_str == "writer") return AgentRole::WRITER;
    if (role_str == "critic") return AgentRole::CRITIC;
    if (role_str == "executor") return AgentRole::EXECUTOR;
    if (role_str == "coordinator") return AgentRole::COORDINATOR;
    if (role_str == "specialist") return AgentRole::SPECIALIST;
    if (role_str == "assistant") return AgentRole::ASSISTANT;
    return AgentRole::GENERIC;
}

std::string AgentRoleManager::specialization_to_string(AgentSpecialization spec) const {
    switch (spec) {
        case AgentSpecialization::NONE: return "none";
        case AgentSpecialization::DATA_ANALYSIS: return "data_analysis";
        case AgentSpecialization::TEXT_PROCESSING: return "text_processing";
        case AgentSpecialization::CODE_GENERATION: return "code_generation";
        case AgentSpecialization::DOCUMENT_ANALYSIS: return "document_analysis";
        case AgentSpecialization::WEB_RESEARCH: return "web_research";
        case AgentSpecialization::REASONING: return "reasoning";
        case AgentSpecialization::PLANNING: return "planning";
        case AgentSpecialization::EXECUTION: return "execution";
        default: return "unknown";
    }
}

AgentSpecialization AgentRoleManager::string_to_specialization(const std::string& spec_str) const {
    if (spec_str == "none") return AgentSpecialization::NONE;
    if (spec_str == "data_analysis") return AgentSpecialization::DATA_ANALYSIS;
    if (spec_str == "text_processing") return AgentSpecialization::TEXT_PROCESSING;
    if (spec_str == "code_generation") return AgentSpecialization::CODE_GENERATION;
    if (spec_str == "document_analysis") return AgentSpecialization::DOCUMENT_ANALYSIS;
    if (spec_str == "web_research") return AgentSpecialization::WEB_RESEARCH;
    if (spec_str == "reasoning") return AgentSpecialization::REASONING;
    if (spec_str == "planning") return AgentSpecialization::PLANNING;
    if (spec_str == "execution") return AgentSpecialization::EXECUTION;
    return AgentSpecialization::NONE;
}

void AgentRoleManager::initialize_default_roles() {
    // RESEARCHER role
    {
        AgentRoleDefinition researcher(AgentRole::RESEARCHER, "Researcher", 
            "Specialized in information gathering and research tasks");
        
        researcher.capabilities = {
            AgentCapability("web_search", "Search the web for information", CapabilityLevel::ADVANCED),
            AgentCapability("document_analysis", "Analyze and extract information from documents", CapabilityLevel::ADVANCED),
            AgentCapability("data_retrieval", "Retrieve data from various sources", CapabilityLevel::EXPERT),
            AgentCapability("fact_checking", "Verify information accuracy", CapabilityLevel::INTERMEDIATE)
        };
        
        researcher.specializations = {AgentSpecialization::WEB_RESEARCH, AgentSpecialization::DOCUMENT_ANALYSIS};
        researcher.default_functions = {"web_search", "context_retrieval", "parse_pdf", "parse_docx"};
        
        role_definitions[AgentRole::RESEARCHER] = std::move(researcher);
    }
    
    // ANALYST role
    {
        AgentRoleDefinition analyst(AgentRole::ANALYST, "Analyst", 
            "Specialized in data analysis and pattern recognition");
        
        analyst.capabilities = {
            AgentCapability("data_analysis", "Analyze complex data sets", CapabilityLevel::EXPERT),
            AgentCapability("pattern_recognition", "Identify patterns and trends", CapabilityLevel::ADVANCED),
            AgentCapability("statistical_analysis", "Perform statistical operations", CapabilityLevel::ADVANCED),
            AgentCapability("visualization", "Create data visualizations", CapabilityLevel::INTERMEDIATE)
        };
        
        analyst.specializations = {AgentSpecialization::DATA_ANALYSIS, AgentSpecialization::REASONING};
        analyst.default_functions = {"data_analysis", "data_transform", "text_analysis"};
        
        role_definitions[AgentRole::ANALYST] = std::move(analyst);
    }
    
    // WRITER role
    {
        AgentRoleDefinition writer(AgentRole::WRITER, "Writer", 
            "Specialized in content creation and text generation");
        
        writer.capabilities = {
            AgentCapability("content_creation", "Generate high-quality content", CapabilityLevel::EXPERT),
            AgentCapability("text_processing", "Process and refine text", CapabilityLevel::ADVANCED),
            AgentCapability("summarization", "Create concise summaries", CapabilityLevel::ADVANCED),
            AgentCapability("editing", "Edit and improve text quality", CapabilityLevel::INTERMEDIATE)
        };
        
        writer.specializations = {AgentSpecialization::TEXT_PROCESSING};
        writer.default_functions = {"text_processing", "code_generation"};
        
        role_definitions[AgentRole::WRITER] = std::move(writer);
    }
    
    // CRITIC role
    {
        AgentRoleDefinition critic(AgentRole::CRITIC, "Critic", 
            "Specialized in evaluation and quality assessment");
        
        critic.capabilities = {
            AgentCapability("quality_assessment", "Evaluate work quality", CapabilityLevel::EXPERT),
            AgentCapability("error_detection", "Find errors and issues", CapabilityLevel::ADVANCED),
            AgentCapability("improvement_suggestions", "Suggest improvements", CapabilityLevel::ADVANCED),
            AgentCapability("validation", "Validate outputs and results", CapabilityLevel::INTERMEDIATE)
        };
        
        critic.specializations = {AgentSpecialization::REASONING};
        critic.default_functions = {"text_analysis"};
        
        role_definitions[AgentRole::CRITIC] = std::move(critic);
    }
    
    // EXECUTOR role
    {
        AgentRoleDefinition executor(AgentRole::EXECUTOR, "Executor", 
            "Specialized in task execution and action taking");
        
        executor.capabilities = {
            AgentCapability("task_execution", "Execute complex tasks", CapabilityLevel::EXPERT),
            AgentCapability("action_taking", "Take concrete actions", CapabilityLevel::ADVANCED),
            AgentCapability("workflow_management", "Manage workflows", CapabilityLevel::ADVANCED),
            AgentCapability("resource_management", "Manage resources efficiently", CapabilityLevel::INTERMEDIATE)
        };
        
        executor.specializations = {AgentSpecialization::EXECUTION, AgentSpecialization::PLANNING};
        executor.default_functions = {"add_document", "remove_document", "inference"};
        
        role_definitions[AgentRole::EXECUTOR] = std::move(executor);
    }
    
    // COORDINATOR role
    {
        AgentRoleDefinition coordinator(AgentRole::COORDINATOR, "Coordinator", 
            "Specialized in orchestration and coordination of multiple agents");
        
        coordinator.capabilities = {
            AgentCapability("agent_orchestration", "Coordinate multiple agents", CapabilityLevel::EXPERT),
            AgentCapability("workflow_design", "Design complex workflows", CapabilityLevel::ADVANCED),
            AgentCapability("resource_allocation", "Allocate resources optimally", CapabilityLevel::ADVANCED),
            AgentCapability("conflict_resolution", "Resolve agent conflicts", CapabilityLevel::INTERMEDIATE)
        };
        
        coordinator.specializations = {AgentSpecialization::PLANNING, AgentSpecialization::EXECUTION};
        coordinator.default_functions = {"tool_discovery"};
        
        role_definitions[AgentRole::COORDINATOR] = std::move(coordinator);
    }
    
    // GENERIC role (default)
    {
        AgentRoleDefinition generic(AgentRole::GENERIC, "Generic", 
            "General-purpose agent with basic capabilities");
        
        generic.capabilities = {
            AgentCapability("basic_processing", "Basic data processing", CapabilityLevel::BASIC),
            AgentCapability("simple_tasks", "Execute simple tasks", CapabilityLevel::BASIC)
        };
        
        generic.specializations = {AgentSpecialization::NONE};
        generic.default_functions = {"echo", "add"};
        
        role_definitions[AgentRole::GENERIC] = std::move(generic);
    }
}

} // namespace kolosal::agents

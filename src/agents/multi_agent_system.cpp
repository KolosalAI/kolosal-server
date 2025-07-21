// File: src/agents/multi_agent_system.cpp
#include "kolosal/agents/multi_agent_system.hpp"
#include "kolosal/agents/builtin_functions.hpp"
#include "kolosal/agents/server_logger_adapter.hpp"
#include "kolosal/agents/agent_core.hpp"
#include "kolosal/agents/message_router.hpp"
#include "kolosal/agents/yaml_config.hpp"
#include "kolosal/agents/agent_config_validator.hpp"
#include <mutex>
#include <chrono>
#include <thread>
#include <sstream>
#include <algorithm>

namespace kolosal::agents {

// ConfigurableAgentFactory implementation
ConfigurableAgentFactory::ConfigurableAgentFactory(std::shared_ptr<Logger> log) 
    : logger(log) {
}

void ConfigurableAgentFactory::register_function_config(const FunctionConfig& config) {
    function_configs[config.name] = config;
    logger->info("Registered function config: " + config.name + " (type: " + config.type + ")");
}

std::unique_ptr<AgentFunction> ConfigurableAgentFactory::create_function(const std::string& function_name) {
    auto it = function_configs.find(function_name);
    if (it == function_configs.end()) {
        logger->error("Function config not found: " + function_name);
        return nullptr;
    }

    const FunctionConfig& config = it->second;
    
    if (config.type == "llm") {
        LLMConfig llm_config; // Use default or parse from config
        return std::make_unique<LLMFunction>(
            config.name, 
            config.description,
            "You are a helpful AI assistant performing the function: " + config.description,
            llm_config
        );
    } else if (config.type == "external_api") {
        return std::make_unique<ExternalAPIFunction>(
            config.name, 
            config.description,
            config.endpoint
        );
    } else if (config.type == "builtin") {
        return create_builtin_function(config);
    } else if (config.type == "inference") {
        return std::make_unique<InferenceFunction>();
    } else if (config.type == "retrieval") {
        return std::make_unique<RetrievalFunction>();
    } else if (config.type == "context_retrieval") {
        return std::make_unique<ContextRetrievalFunction>();
    }

    logger->warn("Unknown function type: " + config.type);
    return nullptr;
}

std::unique_ptr<AgentFunction> ConfigurableAgentFactory::create_builtin_function(const FunctionConfig& config) {
    if (config.name == "add") {
        return std::make_unique<AddFunction>();
    } else if (config.name == "echo") {
        return std::make_unique<EchoFunction>();
    } else if (config.name == "delay") {
        return std::make_unique<DelayFunction>();
    } else if (config.name == "text_analysis" || config.name == "text_processing") {
        return std::make_unique<TextAnalysisFunction>();
    } else if (config.name == "data_analysis") {
        return std::make_unique<DataAnalysisFunction>();
    } else if (config.name == "data_transform") {
        return std::make_unique<DataTransformFunction>();
    } else if (config.name == "inference") {
        return std::make_unique<InferenceFunction>();
    } else if (config.name == "retrieval") {
        return std::make_unique<RetrievalFunction>();
    } else if (config.name == "context_retrieval") {
        return std::make_unique<ContextRetrievalFunction>();
    } else if (config.name == "add_document") {
        return std::make_unique<AddDocumentFunction>();
    } else if (config.name == "remove_document") {
        return std::make_unique<RemoveDocumentFunction>();
    } else if (config.name == "parse_pdf") {
        return std::make_unique<ParsePdfFunction>();
    } else if (config.name == "parse_docx") {
        return std::make_unique<ParseDocxFunction>();
    } else if (config.name == "get_embedding") {
        return std::make_unique<GetEmbeddingFunction>();
    } else if (config.name == "test_document_service") {
        return std::make_unique<TestDocumentServiceFunction>();
    }
    
    logger->warn("Unknown builtin function: " + config.name);
    return nullptr;
}

// YAMLConfigurableAgentManager implementation
YAMLConfigurableAgentManager::YAMLConfigurableAgentManager() {
    // Use the adapter to bridge ServerLogger singleton with agents::Logger interface
    logger = std::make_shared<ServerLoggerAdapter>();
    message_router = std::make_shared<MessageRouter>(logger);
    agent_factory = std::make_shared<ConfigurableAgentFactory>(logger);
}

YAMLConfigurableAgentManager::~YAMLConfigurableAgentManager() {
    stop();
}

bool YAMLConfigurableAgentManager::load_configuration(const std::string& yaml_file) {
    try {
        system_config = SystemConfig::from_file(yaml_file);
        
        // Validate configuration before proceeding
        logger->info("Validating agent system configuration...");
        
        auto system_validation = AgentConfigValidator::validate_system_config(system_config);
        auto engine_validation = AgentConfigValidator::validate_inference_engines(system_config.inference_engines);
        auto function_validation = AgentConfigValidator::validate_function_configs(system_config.functions);
        
        // Log validation results
        if (!system_validation.is_valid || !engine_validation.is_valid || !function_validation.is_valid) {
            logger->error("Configuration validation failed:");
            
            for (const auto& error : system_validation.errors) {
                logger->error("System config error: " + error);
            }
            for (const auto& error : engine_validation.errors) {
                logger->error("Engine config error: " + error);
            }
            for (const auto& error : function_validation.errors) {
                logger->error("Function config error: " + error);
            }
            
            return false;
        }
        
        // Log warnings
        for (const auto& warning : system_validation.warnings) {
            logger->warn("System config warning: " + warning);
        }
        for (const auto& warning : engine_validation.warnings) {
            logger->warn("Engine config warning: " + warning);
        }
        for (const auto& warning : function_validation.warnings) {
            logger->warn("Function config warning: " + warning);
        }
        
        // Log suggestions
        for (const auto& suggestion : system_validation.suggestions) {
            logger->info("System config suggestion: " + suggestion);
        }
        for (const auto& suggestion : engine_validation.suggestions) {
            logger->info("Engine config suggestion: " + suggestion);
        }
        for (const auto& suggestion : function_validation.suggestions) {
            logger->info("Function config suggestion: " + suggestion);
        }
        
        // Validate individual agent configurations
        int valid_agents = 0;
        for (const auto& agent_config : system_config.agents) {
            auto agent_validation = AgentConfigValidator::validate_agent_config(agent_config);
            if (!agent_validation.is_valid) {
                logger->error("Agent '" + agent_config.name + "' configuration is invalid:");
                for (const auto& error : agent_validation.errors) {
                    logger->error("  " + error);
                }
            } else {
                valid_agents++;
                
                // Log warnings for this agent
                for (const auto& warning : agent_validation.warnings) {
                    logger->warn("Agent '" + agent_config.name + "': " + warning);
                }
            }
            
            // Check agent dependencies
            if (!AgentConfigValidator::validate_agent_dependencies(agent_config, system_config.functions)) {
                logger->warn("Agent '" + agent_config.name + "' has missing function dependencies");
            }
        }
        
        if (valid_agents == 0) {
            logger->error("No valid agent configurations found");
            return false;
        }
        
        logger->info("Configuration validation completed: " + std::to_string(valid_agents) + "/" + 
                    std::to_string(system_config.agents.size()) + " agents are valid");
        
        // Check inference engine health - wrap in try-catch to prevent failures
        try {
            logger->info("DEBUG: About to check inference engine health");
            auto engine_statuses = AgentConfigValidator::check_inference_engine_health();
            logger->info("DEBUG: Inference engine health check completed successfully");
            
            int healthy_engines = 0;
            for (const auto& status : engine_statuses) {
                if (status.available && status.healthy) {
                    healthy_engines++;
                    logger->info("Inference engine '" + status.name + "': " + status.status_message);
                } else if (status.available) {
                    logger->warn("Inference engine '" + status.name + "': " + status.status_message);
                } else {
                    logger->debug("Inference engine '" + status.name + "': " + status.status_message);
                }
            }
            
            if (healthy_engines == 0) {
                logger->warn("No healthy inference engines detected - LLM functions may not work properly");
            } else {
                logger->info("Found " + std::to_string(healthy_engines) + " healthy inference engine(s)");
            }
        } catch (const std::exception& e) {
            logger->warn("Failed to check inference engine health: " + std::string(e.what()));
            logger->warn("Continuing with configuration loading...");
        } catch (...) {
            logger->warn("Unknown exception while checking inference engine health");
            logger->warn("Continuing with configuration loading...");
        }
        
        // Register function configurations
        logger->info("DEBUG: About to register function configurations");
        try {
            for (const auto& func_config : system_config.functions) {
                logger->debug("Registering function: " + func_config.name);
                agent_factory->register_function_config(func_config);
            }
            logger->info("DEBUG: Function configurations registered successfully");
        } catch (const std::exception& e) {
            logger->error("Exception registering function configurations: " + std::string(e.what()));
            throw;
        }
        
        logger->info("Configuration loaded successfully from: " + yaml_file);
        logger->info("Found " + std::to_string(system_config.agents.size()) + " agent configurations");
        logger->info("Found " + std::to_string(system_config.functions.size()) + " function configurations");
        logger->info("Found " + std::to_string(system_config.inference_engines.size()) + " inference engine configurations");
        
        logger->info("DEBUG: About to return true from load_configuration");
        return true;
    } catch (const std::exception& e) {
        logger->error("Failed to load configuration: " + std::string(e.what()));
        logger->error("DEBUG: About to return false from load_configuration due to exception");
        return false;
    }
}

void YAMLConfigurableAgentManager::start() {
    if (running.load()) {
        logger->warn("Agent manager is already running");
        return;
    }

    running.store(true);
    message_router->start();
    
    // Create and start agents from configuration
    for (const auto& agent_config : system_config.agents) {
        std::string agent_id = create_agent_from_config(agent_config);
        if (!agent_id.empty() && agent_config.auto_start) {
            start_agent(agent_id);
        }
    }
    
    logger->info("YAML-configurable agent manager started");
}

void YAMLConfigurableAgentManager::stop() {
    if (!running.load()) {
        return;
    }

    logger->info("Stopping YAML-configurable agent manager");
    running.store(false);

    {
        std::lock_guard<std::mutex> lock(agents_mutex);
        // Stop all agents
        for (const auto& pair : active_agents) {
            try {
                if (pair.second && pair.second->is_running()) {
                    pair.second->stop();
                }
            } catch (const std::exception& e) {
                logger->error("Error stopping agent " + pair.first + ": " + e.what());
            }
        }
    }

    message_router->stop();
    logger->info("YAML-configurable agent manager stopped");
}

std::string YAMLConfigurableAgentManager::create_agent_from_config(const AgentConfig& config) {
    if (config.name.empty() || config.type.empty()) {
        logger->error("Invalid agent configuration: name and type are required");
        return "";
    }

    try {
        auto agent = std::make_shared<AgentCore>(config.name, config.type);
        
        // Set up agent capabilities
        for (const auto& capability : config.capabilities) {
            agent->add_capability(capability);
        }
        
        // Register functions for this agent
        for (const auto& function_name : config.functions) {
            auto function = agent_factory->create_function(function_name);
            if (function) {
                agent->get_function_manager()->register_function(std::move(function));
            } else {
                logger->warn("Failed to create function: " + function_name + " for agent: " + config.name);
            }
        }
        
        // Set up message handling
        agent->set_message_router(message_router);
        
        // Get agent ID before locking to minimize lock time
        std::string agent_id = agent->get_agent_id();
        
        // Store the agent thread-safely
        {
            std::lock_guard<std::mutex> lock(agents_mutex);
            active_agents[agent_id] = agent;
        }
        
        logger->info("Created agent from config: " + config.name + " (ID: " + agent_id.substr(0, 8) + "...)");
        return agent_id;
        
    } catch (const std::exception& e) {
        logger->error("Failed to create agent from config: " + config.name + ": " + std::string(e.what()));
        return "";
    }
}

bool YAMLConfigurableAgentManager::start_agent(const std::string& agent_id) {
    if (agent_id.empty()) {
        logger->error("Invalid agent ID provided");
        return false;
    }

    std::lock_guard<std::mutex> lock(agents_mutex);
    auto it = active_agents.find(agent_id);
    if (it == active_agents.end()) {
        logger->error("Agent not found: " + agent_id);
        return false;
    }

    if (!it->second) {
        logger->error("Agent instance is null: " + agent_id);
        return false;
    }

    if (it->second->is_running()) {
        logger->warn("Agent is already running: " + agent_id);
        return true;
    }

    try {
        it->second->start();
        logger->info("Agent started: " + agent_id.substr(0, 8) + "...");
        return true;
    } catch (const std::exception& e) {
        logger->error("Failed to start agent " + agent_id + ": " + e.what());
        return false;
    }
}

bool YAMLConfigurableAgentManager::stop_agent(const std::string& agent_id) {
    if (agent_id.empty()) {
        logger->error("Invalid agent ID provided");
        return false;
    }

    std::lock_guard<std::mutex> lock(agents_mutex);
    auto it = active_agents.find(agent_id);
    if (it == active_agents.end()) {
        logger->error("Agent not found: " + agent_id);
        return false;
    }

    if (!it->second) {
        logger->error("Agent instance is null: " + agent_id);
        return false;
    }

    if (!it->second->is_running()) {
        logger->warn("Agent is not running: " + agent_id);
        return true;
    }

    try {
        it->second->stop();
        logger->info("Agent stopped: " + agent_id.substr(0, 8) + "...");
        return true;
    } catch (const std::exception& e) {
        logger->error("Failed to stop agent " + agent_id + ": " + e.what());
        return false;
    }
}

bool YAMLConfigurableAgentManager::delete_agent(const std::string& agent_id) {
    if (agent_id.empty()) {
        logger->error("Invalid agent ID provided");
        return false;
    }

    std::lock_guard<std::mutex> lock(agents_mutex);
    auto it = active_agents.find(agent_id);
    if (it == active_agents.end()) {
        logger->error("Agent not found: " + agent_id);
        return false;
    }

    if (!it->second) {
        logger->error("Agent instance is null: " + agent_id);
        return false;
    }

    try {
        // Stop the agent first if it's running
        if (it->second->is_running()) {
            it->second->stop();
        }
        
        // Remove from active agents map
        active_agents.erase(it);
        
        logger->info("Agent deleted: " + agent_id.substr(0, 8) + "...");
        return true;
    } catch (const std::exception& e) {
        logger->error("Failed to delete agent " + agent_id + ": " + e.what());
        return false;
    }
}

bool YAMLConfigurableAgentManager::reload_configuration(const std::string& yaml_file) {
    logger->info("Reloading configuration from: " + yaml_file);
    
    std::lock_guard<std::mutex> lock(agents_mutex);
    
    // Stop all current agents
    for (const auto& pair : active_agents) {
        try {
            if (pair.second && pair.second->is_running()) {
                pair.second->stop();
            }
        } catch (const std::exception& e) {
            logger->error("Error stopping agent " + pair.first + ": " + e.what());
        }
    }
    active_agents.clear();
    
    // Load new configuration
    if (!load_configuration(yaml_file)) {
        logger->error("Failed to reload configuration");
        return false;
    }
    
    // Create and start new agents
    for (const auto& agent_config : system_config.agents) {
        std::string agent_id = create_agent_from_config(agent_config);
        if (!agent_id.empty() && agent_config.auto_start) {
            start_agent(agent_id);
        }
    }
    
    logger->info("Configuration reloaded successfully");
    return true;
}

std::vector<std::string> YAMLConfigurableAgentManager::list_agents() const {
    std::vector<std::string> agent_ids;
    std::lock_guard<std::mutex> lock(agents_mutex);
    
    for (const auto& pair : active_agents) {
        agent_ids.push_back(pair.first);
    }
    return agent_ids;
}

std::shared_ptr<AgentCore> YAMLConfigurableAgentManager::get_agent(const std::string& agent_id) {
    if (agent_id.empty()) {
        logger->error("Invalid agent ID provided");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(agents_mutex);
    auto it = active_agents.find(agent_id);
    return (it != active_agents.end()) ? it->second : nullptr;
}

std::string YAMLConfigurableAgentManager::get_system_status() const {
    std::lock_guard<std::mutex> lock(agents_mutex);
    
    std::ostringstream status;
    status << "=== YAML-Configurable Agent Manager Status ===\n";
    status << "Total Agents: " << active_agents.size() << "\n";
    status << "Running Agents: ";
    
    int running_count = 0;
    for (const auto& pair : active_agents) {
        if (pair.second && pair.second->is_running()) {
            running_count++;
        }
    }
    status << running_count << "\n";
    
    status << "Loaded Functions: " << system_config.functions.size() << "\n";
    status << "Worker Threads: " << system_config.worker_threads << "\n";
    status << "Log Level: " << system_config.log_level << "\n";

    return status.str();
}

void YAMLConfigurableAgentManager::demonstrate_system() {
    logger->info("=== YAML-Configurable Multi-Agent System Demo ===");
    
    // Show system status
    logger->info(get_system_status());
    
    // List all agents - Note: This already includes mutex locking
    auto agent_ids = list_agents();
    logger->info("Active Agents: " + std::to_string(agent_ids.size()));
    
    // No need for additional lock since we're using get_agent which has its own locking
    for (const auto& agent_id : agent_ids) {
        auto agent = get_agent(agent_id);
        if (agent) {
            std::ostringstream agent_info;
            agent_info << "  - " << agent->get_agent_name() 
                      << " (ID: " << agent_id.substr(0, 8) << "...)" 
                      << " Type: " << agent->get_agent_type()
                      << " Status: " << (agent->is_running() ? "RUNNING" : "STOPPED");
            logger->info(agent_info.str());
            
            // Show capabilities
            auto capabilities = agent->get_capabilities();
            if (!capabilities.empty()) {
                std::ostringstream caps;
                caps << "    Capabilities: ";
                for (size_t i = 0; i < capabilities.size(); ++i) {
                    if (i > 0) caps << ", ";
                    caps << capabilities[i];
                }
                logger->info(caps.str());
            }
            
            // Show available functions
            auto function_names = agent->get_function_manager()->get_function_names();
            if (!function_names.empty()) {
                std::ostringstream funcs;
                funcs << "    Functions: ";
                for (size_t i = 0; i < function_names.size(); ++i) {
                    if (i > 0) funcs << ", ";
                    funcs << function_names[i];
                }
                logger->info(funcs.str());
            }
        }
    }
    
    logger->info("=== Demo completed ===");
}

} // namespace kolosal::agents
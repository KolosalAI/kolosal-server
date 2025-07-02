#include "kolosal/cli_interface.hpp"
#include "kolosal/cli_advanced.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"

#ifdef KOLOSAL_AGENTS_ENABLED
#include "kolosal/agents/agent_data.hpp"
#include "kolosal/agents/multi_agent_system.hpp"
#include "kolosal/agents/agent_orchestrator.hpp"
#endif

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace kolosal {
    namespace cli {

        // Utility functions for input handling
        namespace {
            std::string trim(const std::string& str) {
                auto start = str.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) return "";
                auto end = str.find_last_not_of(" \t\r\n");
                return str.substr(start, end - start + 1);
            }

            std::vector<std::string> split(const std::string& str, char delimiter = ' ') {
                std::vector<std::string> tokens;
                std::stringstream ss(str);
                std::string token;
                
                while (std::getline(ss, token, delimiter)) {
                    token = trim(token);
                    if (!token.empty()) {
                        tokens.push_back(token);
                    }
                }
                return tokens;
            }

            void enableColors() {
#ifdef _WIN32
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                DWORD dwMode = 0;
                GetConsoleMode(hOut, &dwMode);
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
#endif
            }

            // ANSI color codes
            const std::string RESET = "\033[0m";
            const std::string BOLD = "\033[1m";
            const std::string GREEN = "\033[32m";
            const std::string BLUE = "\033[34m";
            const std::string YELLOW = "\033[33m";
            const std::string RED = "\033[31m";
            const std::string CYAN = "\033[36m";
            const std::string MAGENTA = "\033[35m";
        }

        CLIInterface::CLIInterface() {
            enableColors();
            
            // Register built-in commands
            registerCommand(std::make_unique<HelpCommand>(this));
            registerCommand(std::make_unique<StatusCommand>());
            registerCommand(std::make_unique<ModelsCommand>());
            registerCommand(std::make_unique<ChatCommand>());
            registerCommand(std::make_unique<ConfigCommand>());
            registerCommand(std::make_unique<ExitCommand>(this));
            
#ifdef KOLOSAL_AGENTS_ENABLED
            // Register agent-specific commands
            registerCommand(std::make_unique<AgentsCommand>());
            registerCommand(std::make_unique<WorkflowCommand>());
            registerCommand(std::make_unique<AgentExecuteCommand>());
            registerCommand(std::make_unique<AgentChatCommand>());
            registerCommand(std::make_unique<OrchestrationCommand>());
            registerCommand(std::make_unique<SequentialWorkflowCommand>());
#endif
            
            // Register advanced commands
            registerCommand(std::make_unique<InferenceCommand>());
            registerCommand(std::make_unique<InteractiveChatCommand>());
            registerCommand(std::make_unique<FileProcessCommand>());
            registerCommand(std::make_unique<WorkflowExecuteCommand>());
            registerCommand(std::make_unique<SystemInfoCommand>());
        }

        CLIInterface::~CLIInterface() {
            stop();
        }

        void CLIInterface::printWelcome() {
            std::cout << BOLD << CYAN << "\n";
            std::cout << "+==============================================================+\n";
            std::cout << "|                    " << GREEN << "KOLOSAL CLI" << CYAN << "                         |\n";
            std::cout << "|              " << YELLOW << "AI-Powered Server Interface" << CYAN << "                |\n";
            std::cout << "+==============================================================+" << RESET << "\n\n";
            
            std::cout << GREEN << "Welcome to Kolosal CLI!" << RESET << "\n";
            std::cout << "Type " << BOLD << "help" << RESET << " to see available commands.\n";
            std::cout << "Type " << BOLD << "exit" << RESET << " to quit.\n\n";
        }

        void CLIInterface::start() {
            if (running_) return;
            
            running_ = true;
            printWelcome();
            
            std::string input;
            while (running_) {
                std::cout << BOLD << BLUE << prompt_ << RESET;
                std::getline(std::cin, input);
                
                if (std::cin.eof()) {
                    std::cout << "\n" << YELLOW << "EOF detected. Exiting..." << RESET << "\n";
                    break;
                }
                
                input = trim(input);
                if (input.empty()) continue;
                
                // Add to history
                history_.push_back(input);
                if (history_.size() > 1000) { // Keep history reasonable
                    history_.erase(history_.begin());
                }
                
                // Handle special commands
                if (input[0] == '/') {
                    handleSlashCommand(input);
                    continue;
                }
                
                if (input[0] == '@') {
                    handleAtCommand(input);
                    continue;
                }
                
                if (input[0] == '!') {
                    handleShellCommand(input);
                    continue;
                }
                
                // Execute regular command
                auto result = executeCommand(input);
                if (!result.success && !result.message.empty()) {
                    std::cout << RED << "Error: " << result.message << RESET << "\n";
                }
            }
        }

        void CLIInterface::stop() {
            running_ = false;
        }

        void CLIInterface::registerCommand(std::unique_ptr<Command> command) {
            std::string name = command->getName();
            commands_[name] = std::move(command);
        }

        std::vector<std::string> CLIInterface::parseCommand(const std::string& input) {
            std::vector<std::string> args;
            std::string current;
            bool inQuotes = false;
            bool escape = false;
            
            for (char c : input) {
                if (escape) {
                    current += c;
                    escape = false;
                    continue;
                }
                
                if (c == '\\') {
                    escape = true;
                    continue;
                }
                
                if (c == '"') {
                    inQuotes = !inQuotes;
                    continue;
                }
                
                if (c == ' ' && !inQuotes) {
                    if (!current.empty()) {
                        args.push_back(current);
                        current.clear();
                    }
                    continue;
                }
                
                current += c;
            }
            
            if (!current.empty()) {
                args.push_back(current);
            }
            
            return args;
        }

        CommandResult CLIInterface::executeCommand(const std::string& commandLine) {
            auto args = parseCommand(commandLine);
            if (args.empty()) {
                return {false, "No command specified"};
            }
            
            std::string commandName = args[0];
            args.erase(args.begin()); // Remove command name from args
            
            auto it = commands_.find(commandName);
            if (it == commands_.end()) {
                return {false, "Unknown command: " + commandName + ". Type 'help' for available commands."};
            }
            
            try {
                auto result = it->second->execute(args);
                if (result.success && !result.message.empty()) {
                    std::cout << result.message << "\n";
                }
                if (!result.data.empty()) {
                    std::cout << result.data << "\n";
                }
                return result;
            } catch (const std::exception& e) {
                return {false, "Command execution failed: " + std::string(e.what())};
            }
        }

        void CLIInterface::handleSlashCommand(const std::string& input) {
            std::string command = input.substr(1); // Remove the '/'
            
            if (command == "help" || command == "?") {
                printHelp();
            } else if (command == "clear") {
#ifdef _WIN32
                system("cls");
#else
                system("clear");
#endif
            } else if (command == "history") {
                std::cout << CYAN << "Command History:" << RESET << "\n";
                for (size_t i = 0; i < history_.size(); ++i) {
                    std::cout << "  " << (i + 1) << ": " << history_[i] << "\n";
                }
            } else if (command == "version") {
                std::cout << BOLD << "Kolosal Server CLI v1.0.0" << RESET << "\n";
                std::cout << "Built with <3 for AI development\n";
            } else {
                std::cout << RED << "Unknown slash command: " << command << RESET << "\n";
                std::cout << "Available slash commands: /help, /clear, /history, /version\n";
            }
        }

        void CLIInterface::handleAtCommand(const std::string& input) {
            std::string filepath = input.substr(1); // Remove the '@'
            filepath = trim(filepath);
            
            if (filepath.empty()) {
                std::cout << RED << "No file specified after @" << RESET << "\n";
                return;
            }
            
            // TODO: Implement file reading and context injection
            std::cout << YELLOW << "File context injection: " << filepath << RESET << "\n";
            std::cout << "Note: File context injection will be implemented in a future version.\n";
        }

        void CLIInterface::handleShellCommand(const std::string& input) {
            std::string command = input.substr(1); // Remove the '!'
            command = trim(command);
            
            if (command.empty()) {
                std::cout << YELLOW << "Shell mode not implemented yet. Use !<command> to execute shell commands." << RESET << "\n";
                return;
            }
            
            std::cout << CYAN << "Executing: " << command << RESET << "\n";
            int result = system(command.c_str());
            if (result != 0) {
                std::cout << RED << "Command failed with exit code: " << result << RESET << "\n";
            }
        }

        void CLIInterface::printHelp() {
            std::cout << BOLD << CYAN << "Kolosal CLI Commands:" << RESET << "\n\n";
            
            std::cout << BOLD << "Basic Commands:" << RESET << "\n";
            for (const auto& pair : commands_) {
                const std::string& name = pair.first;
                const std::unique_ptr<Command>& command = pair.second;
                std::cout << "  " << GREEN << name << RESET << " - " << command->getDescription() << "\n";
                std::cout << "    Usage: " << YELLOW << command->getUsage() << RESET << "\n\n";
            }
            
            std::cout << BOLD << "Special Commands:" << RESET << "\n";
            std::cout << "  " << MAGENTA << "/help" << RESET << " or " << MAGENTA << "/?" << RESET << " - Show this help\n";
            std::cout << "  " << MAGENTA << "/clear" << RESET << " - Clear the screen\n";
            std::cout << "  " << MAGENTA << "/history" << RESET << " - Show command history\n";
            std::cout << "  " << MAGENTA << "/version" << RESET << " - Show version info\n\n";
            
            std::cout << "  " << MAGENTA << "@<file>" << RESET << " - Include file content (planned)\n";
            std::cout << "  " << MAGENTA << "!<command>" << RESET << " - Execute shell command\n\n";
        }

        // Built-in command implementations
        CommandResult HelpCommand::execute(const std::vector<std::string>& args) {
            cli_->printHelp();
            return {true, ""};
        }

        CommandResult StatusCommand::execute(const std::vector<std::string>& args) {
            try {
                ServerAPI& server = ServerAPI::instance();
                
                std::stringstream ss;
                ss << BOLD << GREEN << "Server Status" << RESET << "\n";
                ss << "-----------------\n";
                
                // TODO: Add actual status checking
                ss << "Status: " << GREEN << "Running" << RESET << "\n";
                ss << "Port: " << CYAN << "8080" << RESET << "\n"; // TODO: Get actual port
                
                // Node Manager Status
                try {
                    auto& nodeManager = server.getNodeManager();
                    ss << "Node Manager: " << GREEN << "Active" << RESET << "\n";
                } catch (...) {
                    ss << "Node Manager: " << RED << "Error" << RESET << "\n";
                }
                
                // Agent System Status
                try {
                    auto& agentManager = server.getAgentManager();
                    ss << "Agent System: " << GREEN << "Active" << RESET << "\n";
                } catch (...) {
                    ss << "Agent System: " << RED << "Error" << RESET << "\n";
                }
                
                return {true, "", ss.str()};
            } catch (const std::exception& e) {
                return {false, "Failed to get server status: " + std::string(e.what())};
            }
        }

        CommandResult ModelsCommand::execute(const std::vector<std::string>& args) {
            std::stringstream ss;
            ss << BOLD << BLUE << "📦 Model Management" << RESET << "\n";
            ss << "──────────────────\n";
            
            if (args.empty() || args[0] == "list") {
                ss << "Available Models:\n";
                ss << "  • " << GREEN << "Qwen3-0.6B-UD-Q4_K_XL.gguf" << RESET << " (downloaded)\n";
                ss << "\nTo download a model: " << YELLOW << "models download <model_name>" << RESET << "\n";
                ss << "To remove a model: " << YELLOW << "models remove <model_name>" << RESET << "\n";
            } else if (args[0] == "download") {
                if (args.size() < 2) {
                    return {false, "Please specify a model name to download"};
                }
                ss << "Downloading model: " << CYAN << args[1] << RESET << "\n";
                ss << "This feature will be implemented with auto-setup integration.\n";
            } else if (args[0] == "remove") {
                if (args.size() < 2) {
                    return {false, "Please specify a model name to remove"};
                }
                ss << "Removing model: " << CYAN << args[1] << RESET << "\n";
                ss << "This feature will be implemented in a future version.\n";
            } else {
                return {false, "Unknown models subcommand: " + args[0]};
            }
            
            return {true, "", ss.str()};
        }

        CommandResult ChatCommand::execute(const std::vector<std::string>& args) {
            std::stringstream ss;
            ss << BOLD << MAGENTA << "💬 Chat Interface" << RESET << "\n";
            ss << "─────────────────\n";
            
            if (args.empty()) {
                ss << "Starting chat with default model...\n";
                ss << "Type your message and press Enter.\n";
                ss << "Type " << YELLOW << "exit" << RESET << " to return to CLI.\n\n";
                
                // TODO: Implement actual chat interface
                ss << CYAN << "Chat interface will be implemented to connect with the inference engine." << RESET << "\n";
            } else {
                // Direct message mode
                std::string message;
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i > 0) message += " ";
                    message += args[i];
                }
                
                ss << "Sending message: " << CYAN << message << RESET << "\n";
                ss << "Response: " << GREEN << "This will connect to the actual inference engine." << RESET << "\n";
            }
            
            return {true, "", ss.str()};
        }

        CommandResult AgentsCommand::execute(const std::vector<std::string>& args) {
            std::stringstream ss;
            ss << BOLD << YELLOW << "Agent Management" << RESET << "\n";
            ss << "====================\n";
            
            try {
                ServerAPI& server = ServerAPI::instance();
                auto& agentManager = server.getAgentManager();
                
                if (args.empty() || args[0] == "list") {
                    ss << "Available Agents:\n";
                    ss << "+-----------------+--------------------------------+----------+\n";
                    ss << "| Name            | Description                    | Status   |\n";
                    ss << "+-----------------+--------------------------------+----------+\n";
                    
                    // Get actual agent list from the system
                    auto agent_ids = agentManager.list_agents();
                    if (agent_ids.empty()) {
                        ss << "| No agents found                                           |\n";
                    } else {
                        for (const auto& agent_id : agent_ids) {
                            auto agent = agentManager.get_agent(agent_id);
                            if (agent) {
                                std::string name = agent->get_agent_name();
                                std::string type = agent->get_agent_type();
                                std::string status = agent->is_running() ? GREEN + "RUNNING" + RESET : RED + "STOPPED" + RESET;
                                
                                // Truncate long names/descriptions
                                if (name.length() > 15) name = name.substr(0, 12) + "...";
                                if (type.length() > 30) type = type.substr(0, 27) + "...";
                                
                                ss << "| " << std::left << std::setw(15) << name << " | " 
                                   << std::setw(30) << type << " | " << status << " |\n";
                            }
                        }
                    }
                    ss << "+-----------------+--------------------------------+----------+\n";
                    ss << "\nTotal agents: " << CYAN << agent_ids.size() << RESET << "\n";
                    
                } else if (args[0] == "status") {
                    ss << agentManager.get_system_status() << "\n";
                    
                } else if (args[0] == "info") {
                    if (args.size() < 2) {
                        return {false, "Please specify an agent name: agents info <agent_name>"};
                    }
                    
                    std::string agent_name = args[1];
                    auto agent = agentManager.get_agent(agent_name);
                    if (!agent) {
                        return {false, "Agent not found: " + agent_name};
                    }
                    
                    ss << "Agent Information:\n";
                    ss << "==================\n";
                    ss << "Name: " << CYAN << agent->get_agent_name() << RESET << "\n";
                    ss << "ID: " << YELLOW << agent->get_agent_id() << RESET << "\n";
                    ss << "Type: " << GREEN << agent->get_agent_type() << RESET << "\n";
                    ss << "Status: " << (agent->is_running() ? GREEN + "RUNNING" + RESET : RED + "STOPPED" + RESET) << "\n";
                    
                    auto capabilities = agent->get_capabilities();
                    if (!capabilities.empty()) {
                        ss << "Capabilities:\n";
                        for (const auto& cap : capabilities) {
                            ss << "  - " << cap << "\n";
                        }
                    }
                    
                } else if (args[0] == "start") {
                    if (args.size() < 2) {
                        return {false, "Please specify an agent name: agents start <agent_name>"};
                    }
                    
                    std::string agent_name = args[1];
                    if (agentManager.start_agent(agent_name)) {
                        ss << GREEN << "Successfully started agent: " << agent_name << RESET << "\n";
                    } else {
                        return {false, "Failed to start agent: " + agent_name};
                    }
                    
                } else if (args[0] == "stop") {
                    if (args.size() < 2) {
                        return {false, "Please specify an agent name: agents stop <agent_name>"};
                    }
                    
                    std::string agent_name = args[1];
                    if (agentManager.stop_agent(agent_name)) {
                        ss << GREEN << "Successfully stopped agent: " << agent_name << RESET << "\n";
                    } else {
                        return {false, "Failed to stop agent: " + agent_name};
                    }
                    
                } else if (args[0] == "restart") {
                    if (args.size() < 2) {
                        return {false, "Please specify an agent name: agents restart <agent_name>"};
                    }
                    
                    std::string agent_name = args[1];
                    ss << "Restarting agent: " << CYAN << agent_name << RESET << "\n";
                    
                    if (agentManager.stop_agent(agent_name)) {
                        ss << "  - Stopped agent\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        
                        if (agentManager.start_agent(agent_name)) {
                            ss << "  - " << GREEN << "Successfully restarted agent" << RESET << "\n";
                        } else {
                            return {false, "Failed to start agent after stopping: " + agent_name};
                        }
                    } else {
                        return {false, "Failed to stop agent: " + agent_name};
                    }
                    
                } else if (args[0] == "reload") {
                    ss << "Reloading agent configuration...\n";
                    if (agentManager.reload_configuration("config/agents.yaml")) {
                        ss << GREEN << "Successfully reloaded agent configuration" << RESET << "\n";
                    } else {
                        return {false, "Failed to reload agent configuration"};
                    }
                    
                } else if (args[0] == "help") {
                    ss << "Agent Commands:\n";
                    ss << "===============\n";
                    ss << "agents list              - List all agents\n";
                    ss << "agents status            - Show system status\n";
                    ss << "agents info <name>       - Show detailed agent information\n";
                    ss << "agents start <name>      - Start an agent\n";
                    ss << "agents stop <name>       - Stop an agent\n";
                    ss << "agents restart <name>    - Restart an agent\n";
                    ss << "agents reload            - Reload agent configuration\n";
                    ss << "agents help              - Show this help\n";
                    
                } else {
                    return {false, "Unknown agents subcommand: " + args[0] + ". Use 'agents help' for available commands."};
                }
                
                return {true, "", ss.str()};
            } catch (const std::exception& e) {
                return {false, "Failed to access agent system: " + std::string(e.what())};
            }
        }

        CommandResult WorkflowCommand::execute(const std::vector<std::string>& args) {
            std::stringstream ss;
            ss << BOLD << CYAN << "Workflow Management" << RESET << "\n";
            ss << "===================\n";
            
            try {
                ServerAPI& server = ServerAPI::instance();
                auto& orchestrator = server.getAgentOrchestrator();
                
                if (args.empty() || args[0] == "list") {
                    ss << "Available Workflows:\n";
                    ss << "+-------------------+--------------------------------+----------+\n";
                    ss << "| Workflow ID       | Description                    | Status   |\n";
                    ss << "+-------------------+--------------------------------+----------+\n";
                    
                    auto workflows = orchestrator.list_workflows();
                    if (workflows.empty()) {
                        ss << "| No workflows found                                         |\n";
                    } else {
                        for (const auto& workflow : workflows) {
                            std::string id = workflow.workflow_id;
                            std::string desc = workflow.description;
                            std::string status = YELLOW + "READY" + RESET;
                            
                            // Check if workflow is currently running
                            auto result = orchestrator.get_workflow_result(id);
                            if (result.workflow_id == id) {
                                if (result.success) {
                                    status = GREEN + "COMPLETED" + RESET;
                                } else if (!result.error_message.empty()) {
                                    status = RED + "FAILED" + RESET;
                                } else {
                                    status = BLUE + "RUNNING" + RESET;
                                }
                            }
                            
                            // Truncate long strings
                            if (id.length() > 17) id = id.substr(0, 14) + "...";
                            if (desc.length() > 30) desc = desc.substr(0, 27) + "...";
                            
                            ss << "| " << std::left << std::setw(17) << id << " | " 
                               << std::setw(30) << desc << " | " << status << " |\n";
                        }
                    }
                    ss << "+-------------------+--------------------------------+----------+\n";
                    ss << "\nTotal workflows: " << CYAN << workflows.size() << RESET << "\n";
                    
                } else if (args[0] == "create") {
                    if (args.size() < 2) {
                        return {false, "Please specify workflow type: workflow create <sequential|parallel|pipeline>"};
                    }
                    
                    std::string workflow_type = args[1];
                    ss << "Creating " << CYAN << workflow_type << RESET << " workflow...\n";
                    
                    if (workflow_type == "sequential") {
                        ss << "Sequential workflow will execute agents one after another.\n";
                        ss << "Use: workflow execute <workflow_id> to run it.\n";
                    } else if (workflow_type == "parallel") {
                        ss << "Parallel workflow will execute multiple agents simultaneously.\n";
                        ss << "Use: workflow execute <workflow_id> to run it.\n";
                    } else if (workflow_type == "pipeline") {
                        ss << "Pipeline workflow will pass outputs between agents.\n";
                        ss << "Use: workflow execute <workflow_id> to run it.\n";
                    } else {
                        return {false, "Unknown workflow type: " + workflow_type};
                    }
                    
                } else if (args[0] == "execute") {
                    if (args.size() < 2) {
                        return {false, "Please specify a workflow ID: workflow execute <workflow_id>"};
                    }
                    
                    std::string workflow_id = args[1];
                    ss << "Executing workflow: " << CYAN << workflow_id << RESET << "\n";
                    
                    auto result = orchestrator.execute_workflow(workflow_id);
                    if (result.success) {
                        ss << GREEN << "Workflow completed successfully!" << RESET << "\n";
                        ss << "Execution time: " << YELLOW << result.total_execution_time_ms << "ms" << RESET << "\n";
                        
                        if (!result.step_results.empty()) {
                            ss << "\nStep Results:\n";
                            for (const auto& [step_id, step_result] : result.step_results) {
                                ss << "  - " << step_id << ": " << (step_result.success ? GREEN + "SUCCESS" + RESET : RED + "FAILED" + RESET) << "\n";
                                if (!step_result.data.empty()) {
                                    ss << "    Output: " << step_result.data << "\n";
                                }
                            }
                        }
                    } else {
                        return {false, "Workflow execution failed: " + result.error_message};
                    }
                    
                } else if (args[0] == "status") {
                    if (args.size() < 2) {
                        ss << "Overall Workflow Status:\n";
                        ss << "========================\n";
                        auto metrics = orchestrator.get_metrics();
                        ss << "Active workflows: " << BLUE << metrics.active_workflows << RESET << "\n";
                        ss << "Completed workflows: " << GREEN << metrics.completed_workflows << RESET << "\n";
                        ss << "Failed workflows: " << RED << metrics.failed_workflows << RESET << "\n";
                    } else {
                        std::string workflow_id = args[1];
                        auto result = orchestrator.get_workflow_result(workflow_id);
                        
                        if (result.workflow_id.empty()) {
                            return {false, "Workflow not found: " + workflow_id};
                        }
                        
                        ss << "Workflow Status: " << CYAN << workflow_id << RESET << "\n";
                        ss << "=================\n";
                        ss << "Success: " << (result.success ? GREEN + "YES" + RESET : RED + "NO" + RESET) << "\n";
                        ss << "Execution time: " << YELLOW << result.total_execution_time_ms << "ms" << RESET << "\n";
                        
                        if (!result.error_message.empty()) {
                            ss << "Error: " << RED << result.error_message << RESET << "\n";
                        }
                        
                        if (!result.step_results.empty()) {
                            ss << "\nStep Results:\n";
                            for (const auto& [step_id, step_result] : result.step_results) {
                                ss << "  " << step_id << ": " << (step_result.success ? GREEN + "SUCCESS" + RESET : RED + "FAILED" + RESET) << "\n";
                            }
                        }
                    }
                    
                } else if (args[0] == "stop") {
                    if (args.size() < 2) {
                        return {false, "Please specify a workflow ID: workflow stop <workflow_id>"};
                    }
                    
                    std::string workflow_id = args[1];
                    if (orchestrator.cancel_workflow(workflow_id)) {
                        ss << GREEN << "Successfully stopped workflow: " << workflow_id << RESET << "\n";
                    } else {
                        return {false, "Failed to stop workflow: " + workflow_id};
                    }
                    
                } else if (args[0] == "help") {
                    ss << "Workflow Commands:\n";
                    ss << "==================\n";
                    ss << "workflow list                        - List all workflows\n";
                    ss << "workflow create <type>               - Create new workflow\n";
                    ss << "workflow execute <workflow_id>       - Execute a workflow\n";
                    ss << "workflow status [workflow_id]        - Show workflow status\n";
                    ss << "workflow stop <workflow_id>          - Stop running workflow\n";
                    ss << "workflow help                        - Show this help\n";
                    ss << "\nWorkflow Types:\n";
                    ss << "  sequential  - Execute agents one after another\n";
                    ss << "  parallel    - Execute agents simultaneously\n";
                    ss << "  pipeline    - Pass outputs between agents\n";
                    
                } else {
                    return {false, "Unknown workflow subcommand: " + args[0] + ". Use 'workflow help' for available commands."};
                }
                
                return {true, "", ss.str()};
            } catch (const std::exception& e) {
                return {false, "Failed to access workflow system: " + std::string(e.what())};
            }
        }

        CommandResult ConfigCommand::execute(const std::vector<std::string>& args) {
            std::stringstream ss;
            ss << BOLD << GREEN << "⚙️  Configuration" << RESET << "\n";
            ss << "─────────────────\n";
            
            if (args.empty() || args[0] == "show") {
                ss << "Current Configuration:\n";
                ss << "  Port: " << CYAN << "8080" << RESET << "\n";
                ss << "  Auto-setup: " << GREEN << "Enabled" << RESET << "\n";
                ss << "  Agent Discovery: " << GREEN << "Enabled" << RESET << "\n";
                ss << "  Model Path: " << CYAN << "./downloads/" << RESET << "\n";
            } else if (args[0] == "get") {
                if (args.size() < 2) {
                    return {false, "Please specify a configuration key"};
                }
                ss << "Configuration value for '" << args[1] << "': Not implemented\n";
            } else if (args[0] == "set") {
                if (args.size() < 3) {
                    return {false, "Please specify both key and value"};
                }
                ss << "Setting '" << args[1] << "' to '" << args[2] << "': Not implemented\n";
            } else {
                return {false, "Unknown config subcommand: " + args[0]};
            }
            
            return {true, "", ss.str()};
        }

        CommandResult ExitCommand::execute(const std::vector<std::string>& args) {
            std::cout << GREEN << "Goodbye! 👋" << RESET << "\n";
            cli_->stop();
            return {true, ""};
        }

        CommandResult AgentExecuteCommand::execute(const std::vector<std::string>& args) {
#ifdef KOLOSAL_AGENTS_ENABLED
            if (args.size() < 2) {
                return {false, "Usage: execute <agent_name> <function_name> [args...]"};
            }
            
            std::stringstream ss;
            std::string agent_name = args[0];
            std::string function_name = args[1];
            
            ss << "Executing function '" << CYAN << function_name << RESET 
               << "' on agent '" << YELLOW << agent_name << RESET << "'...\n";
            ss << GREEN << "Agent function execution would be implemented here." << RESET << "\n";
            ss << "Parameters: ";
            for (size_t i = 2; i < args.size(); ++i) {
                if (i > 2) ss << ", ";
                ss << args[i];
            }
            ss << "\n";
            
            return {true, "", ss.str()};
#else
            return {false, "Agent system not enabled in this build"};
#endif
        }

        CommandResult AgentChatCommand::execute(const std::vector<std::string>& args) {
#ifdef KOLOSAL_AGENTS_ENABLED
            if (args.empty()) {
                return {false, "Usage: agent-chat <agent_name> [message]"};
            }
            
            std::string agent_name = args[0];
            
            try {
                ServerAPI& server = ServerAPI::instance();
                auto& agentManager = server.getAgentManager();
                
                auto agent = agentManager.get_agent(agent_name);
                if (!agent) {
                    return {false, "Agent not found: " + agent_name};
                }
                
                std::stringstream ss;
                ss << BOLD << BLUE << "Chat with Agent: " << agent_name << RESET << "\n";
                ss << "========================\n";
                
                if (args.size() > 1) {
                    // Single message mode
                    std::string message;
                    for (size_t i = 1; i < args.size(); ++i) {
                        if (i > 1) message += " ";
                        message += args[i];
                    }
                    
                    ss << "You: " << message << "\n";
                    
                    // Send message to agent
                    agents::AgentData input;
        CommandResult AgentChatCommand::execute(const std::vector<std::string>& args) {
#ifdef KOLOSAL_AGENTS_ENABLED
            if (args.empty()) {
                return {false, "Usage: agent-chat <agent_name> [message]"};
            }
            
            std::string agent_name = args[0];
            std::stringstream ss;
            ss << BOLD << BLUE << "Chat with Agent: " << agent_name << RESET << "\n";
            ss << "========================\n";
            
            if (args.size() > 1) {
                std::string message;
                for (size_t i = 1; i < args.size(); ++i) {
                    if (i > 1) message += " ";
                    message += args[i];
                }
                ss << "You: " << message << "\n";
                ss << CYAN << agent_name << ": " << RESET << "This is a simulated response. Agent chat would be implemented here.\n";
            } else {
                ss << GREEN << "Interactive chat mode would be started here." << RESET << "\n";
                ss << "Type 'exit' to end the conversation.\n";
            }
            
            return {true, "", ss.str()};
#else
            return {false, "Agent system not enabled in this build"};
#endif
        }

        CommandResult OrchestrationCommand::execute(const std::vector<std::string>& args) {
#ifdef KOLOSAL_AGENTS_ENABLED
            std::stringstream ss;
            ss << BOLD << MAGENTA << "Agent Orchestration" << RESET << "\n";
            ss << "===================\n";
            
            try {
                ServerAPI& server = ServerAPI::instance();
                auto& orchestrator = server.getAgentOrchestrator();
                
                if (args.empty() || args[0] == "list") {
                    ss << "Collaboration Groups:\n";
                    ss << "+----------------+----------+------------------+\n";
                    ss << "| Group ID       | Pattern  | Agents           |\n";
                    ss << "+----------------+----------+------------------+\n";
                    
                    auto groups = orchestrator.list_collaboration_groups();
                    if (groups.empty()) {
                        ss << "| No collaboration groups found            |\n";
                    } else {
                        for (const auto& group : groups) {
                            std::string pattern_str = "UNKNOWN";
                            
                            std::string group_id = group.group_id;
                            if (group_id.length() > 14) group_id = group_id.substr(0, 11) + "...";
                            
                            std::string agents_str = std::to_string(group.agent_ids.size()) + " agents";
                            
                            ss << "| " << std::left << std::setw(14) << group_id << " | " 
                               << std::setw(8) << pattern_str << " | " 
                               << std::setw(16) << agents_str << " |\n";
                        }
                    }
                    ss << "+----------------+----------+------------------+\n";
                    
                } else if (args[0] == "create") {
                    if (args.size() < 3) {
                        return {false, "Usage: orchestrate create <group_name> <pattern> [agent1,agent2,...]"};
                    }
                    
                    std::string group_name = args[1];
                    std::string pattern_str = args[2];
                    
                    ss << "Creating collaboration group: " << CYAN << group_name << RESET << "\n";
                    ss << "Pattern: " << YELLOW << pattern_str << RESET << "\n";
                    
                    // For now, use a simplified approach without enum dependencies
                    ss << YELLOW << "Creating collaboration group with pattern: " << pattern_str << RESET << "\n";
                    ss << GREEN << "Collaboration group creation would be implemented here" << RESET << "\n";
                    
                } else if (args[0] == "execute") {
                    if (args.size() < 2) {
                        return {false, "Usage: orchestrate execute <group_id> [input_data]"};
                    }
                    
                    std::string group_id = args[1];
                    AgentData input_data;
                    
                    if (args.size() > 2) {
                        input_data.set_string("user_input", args[2]);
                    }
                    
                    ss << "Executing collaboration group: " << CYAN << group_id << RESET << "\n";
                    
                    auto result = orchestrator.execute_collaboration(group_id, input_data);
                    if (result.success) {
                        ss << GREEN << "Collaboration completed successfully!" << RESET << "\n";
                        if (!result.data.empty()) {
                            ss << "Result: " << result.data << "\n";
                        }
                    } else {
                        return {false, "Collaboration execution failed: " + result.error_message};
                    }
                    
                } else if (args[0] == "help") {
                    ss << "Orchestration Commands:\n";
                    ss << "=======================\n";
                    ss << "orchestrate list                           - List collaboration groups\n";
                    ss << "orchestrate create <name> <pattern> [agents] - Create collaboration group\n";
                    ss << "orchestrate execute <group_id> [input]     - Execute collaboration\n";
                    ss << "orchestrate help                           - Show this help\n";
                    ss << "\nPatterns:\n";
                    ss << "  sequential   - Execute agents one after another\n";
                    ss << "  parallel     - Execute agents simultaneously\n";
                    ss << "  pipeline     - Pass outputs between agents\n";
                    ss << "  consensus    - Multiple agents vote on result\n";
                    ss << "  hierarchy    - Master-slave pattern\n";
                    ss << "  negotiation  - Agents negotiate to reach agreement\n";
                    
                } else {
                    return {false, "Unknown orchestration subcommand: " + args[0] + ". Use 'orchestrate help' for available commands."};
                }
                
                return {true, "", ss.str()};
                
            } catch (const std::exception& e) {
                return {false, "Failed to access orchestration system: " + std::string(e.what())};
            }
#else
            return {false, "Agent system not enabled in this build"};
#endif
        }

        CommandResult SequentialWorkflowCommand::execute(const std::vector<std::string>& args) {
#ifdef KOLOSAL_AGENTS_ENABLED
            std::stringstream ss;
            ss << BOLD << GREEN << "Sequential Workflows" << RESET << "\n";
            ss << "====================\n";
            
            try {
                ServerAPI& server = ServerAPI::instance();
                auto& seqWorkflow = server.getSequentialWorkflowExecutor();
                
                if (args.empty() || args[0] == "list") {
                    ss << "Available Sequential Workflows:\n";
                    ss << "+-------------------+--------------------------------+----------+\n";
                    ss << "| Workflow ID       | Description                    | Status   |\n";
                    ss << "+-------------------+--------------------------------+----------+\n";
                    
                    auto workflows = seqWorkflow.list_workflows();
                    if (workflows.empty()) {
                        ss << "| No sequential workflows found                              |\n";
                    } else {
                        for (const auto& [id, workflow] : workflows) {
                            std::string desc = workflow.description;
                            std::string status = YELLOW + "READY" + RESET;
                            
                            // Check status
                            auto result = seqWorkflow.get_workflow_result(id);
                            if (!result.workflow_id.empty()) {
                                if (result.success) {
                                    status = GREEN + "COMPLETED" + RESET;
                                } else if (!result.error_message.empty()) {
                                    status = RED + "FAILED" + RESET;
                                } else {
                                    status = BLUE + "RUNNING" + RESET;
                                }
                            }
                            
                            // Truncate strings
                            std::string display_id = id;
                            if (display_id.length() > 17) display_id = display_id.substr(0, 14) + "...";
                            if (desc.length() > 30) desc = desc.substr(0, 27) + "...";
                            
                            ss << "| " << std::left << std::setw(17) << display_id << " | " 
                               << std::setw(30) << desc << " | " << status << " |\n";
                        }
                    }
                    ss << "+-------------------+--------------------------------+----------+\n";
                    
                } else if (args[0] == "create") {
                    if (args.size() < 2) {
                        return {false, "Usage: seq-workflow create <workflow_name> [description]"};
                    }
                    
                    std::string workflow_name = args[1];
                    std::string description = args.size() > 2 ? args[2] : "Sequential workflow";
                    
                    ss << "Creating sequential workflow: " << CYAN << workflow_name << RESET << "\n";
                    ss << "Description: " << YELLOW << description << RESET << "\n";
                    
                    // Create a simple sequential workflow
                    SequentialWorkflowBuilder builder;
                    builder.set_name(workflow_name)
                           .set_description(description);
                    
                    auto workflow = builder.build();
                    std::string workflow_id = seqWorkflow.register_workflow(workflow);
                    
                    if (!workflow_id.empty()) {
                        ss << GREEN << "Successfully created workflow with ID: " << workflow_id << RESET << "\n";
                    } else {
                        return {false, "Failed to create workflow"};
                    }
                    
                } else if (args[0] == "execute") {
                    if (args.size() < 2) {
                        return {false, "Usage: seq-workflow execute <workflow_id> [input_data]"};
                    }
                    
                    std::string workflow_id = args[1];
                    AgentData input_data;
                    
                    if (args.size() > 2) {
                        input_data.set_string("user_input", args[2]);
                    }
                    
                    ss << "Executing sequential workflow: " << CYAN << workflow_id << RESET << "\n";
                    
                    auto result = seqWorkflow.execute_workflow(workflow_id, input_data);
                    if (result.success) {
                        ss << GREEN << "Sequential workflow completed successfully!" << RESET << "\n";
                        ss << "Execution time: " << YELLOW << result.total_execution_time_ms << "ms" << RESET << "\n";
                        
                        if (!result.final_output.empty()) {
                            ss << "Final output: " << result.final_output << "\n";
                        }
                        
                        if (!result.step_outputs.empty()) {
                            ss << "\nStep Results:\n";
                            for (size_t i = 0; i < result.step_outputs.size(); ++i) {
                                ss << "  Step " << (i + 1) << ": " << result.step_outputs[i] << "\n";
                            }
                        }
                    } else {
                        return {false, "Sequential workflow execution failed: " + result.error_message};
                    }
                    
                } else if (args[0] == "status") {
                    ss << "Sequential Workflow Status:\n";
                    ss << "===========================\n";
                    auto metrics = seqWorkflow.get_metrics();
                    ss << "Active workflows: " << BLUE << metrics.active_workflows << RESET << "\n";
                    ss << "Completed workflows: " << GREEN << metrics.completed_workflows << RESET << "\n";
                    ss << "Failed workflows: " << RED << metrics.failed_workflows << RESET << "\n";
                    
                } else if (args[0] == "help") {
                    ss << "Sequential Workflow Commands:\n";
                    ss << "=============================\n";
                    ss << "seq-workflow list                      - List all sequential workflows\n";
                    ss << "seq-workflow create <name> [desc]      - Create new sequential workflow\n";
                    ss << "seq-workflow execute <id> [input]      - Execute a sequential workflow\n";
                    ss << "seq-workflow status                    - Show execution status\n";
                    ss << "seq-workflow help                      - Show this help\n";
                    
                } else {
                    return {false, "Unknown sequential workflow subcommand: " + args[0] + ". Use 'seq-workflow help' for available commands."};
                }
                
                return {true, "", ss.str()};
                
            } catch (const std::exception& e) {
                return {false, "Failed to access sequential workflow system: " + std::string(e.what())};
            }
#else
            return {false, "Agent system not enabled in this build"};
#endif
        }
    } // namespace cli
} // namespace kolosal

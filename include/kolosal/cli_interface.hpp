#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include "export.hpp"

namespace kolosal {
    namespace cli {

        // Command result structure
        struct CommandResult {
            bool success = false;
            std::string message;
            std::string data;
        };

        // Base class for CLI commands
        class KOLOSAL_SERVER_API Command {
        public:
            virtual ~Command() = default;
            virtual CommandResult execute(const std::vector<std::string>& args) = 0;
            virtual std::string getName() const = 0;
            virtual std::string getDescription() const = 0;
            virtual std::string getUsage() const = 0;
        };

        // CLI Interface main class
        class KOLOSAL_SERVER_API CLIInterface {
        private:
            std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
            std::vector<std::string> history_;
            bool running_ = false;
            std::string prompt_ = "kolosal> ";
            
            // Helper methods
            std::vector<std::string> parseCommand(const std::string& input);
            void printWelcome();
            void showCompletions(const std::string& partial);
            void handleSlashCommand(const std::string& input);
            void handleAtCommand(const std::string& input);
            void handleShellCommand(const std::string& input);
            
        public:
            CLIInterface();
            ~CLIInterface();
            
            // Delete copy constructor and copy assignment operator
            CLIInterface(const CLIInterface&) = delete;
            CLIInterface& operator=(const CLIInterface&) = delete;
            
            // Delete move constructor and move assignment operator
            CLIInterface(CLIInterface&&) = delete;
            CLIInterface& operator=(CLIInterface&&) = delete;

            // Main CLI operations
            void start();
            void stop();
            bool isRunning() const { return running_; }
            
            // Command management
            void registerCommand(std::unique_ptr<Command> command);
            CommandResult executeCommand(const std::string& commandLine);
            
            // Utility methods
            void setPrompt(const std::string& prompt) { prompt_ = prompt; }
            const std::vector<std::string>& getHistory() const { return history_; }
            void clearHistory() { history_.clear(); }
            void printHelp(); // Made public for HelpCommand access
            
            // Built-in command implementations
            CommandResult handleHelp(const std::vector<std::string>& args);
            CommandResult handleHistory(const std::vector<std::string>& args);
            CommandResult handleClear(const std::vector<std::string>& args);
            CommandResult handleExit(const std::vector<std::string>& args);
        };

        // Built-in commands
        class KOLOSAL_SERVER_API HelpCommand : public Command {
        private:
            CLIInterface* cli_;
        public:
            explicit HelpCommand(CLIInterface* cli) : cli_(cli) {}
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "help"; }
            std::string getDescription() const override { return "Show available commands and their usage"; }
            std::string getUsage() const override { return "help [command]"; }
        };

        class KOLOSAL_SERVER_API StatusCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "status"; }
            std::string getDescription() const override { return "Show server status"; }
            std::string getUsage() const override { return "status"; }
        };

        class KOLOSAL_SERVER_API ModelsCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "models"; }
            std::string getDescription() const override { return "List available models"; }
            std::string getUsage() const override { return "models [list|download|remove] [model_name]"; }
        };

        class KOLOSAL_SERVER_API ChatCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "chat"; }
            std::string getDescription() const override { return "Start interactive chat with a model"; }
            std::string getUsage() const override { return "chat [model_name] [message]"; }
        };

        class KOLOSAL_SERVER_API AgentsCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "agents"; }
            std::string getDescription() const override { return "Manage agents"; }
            std::string getUsage() const override { return "agents [list|create|execute] [args...]"; }
        };

        class KOLOSAL_SERVER_API WorkflowCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "workflow"; }
            std::string getDescription() const override { return "Manage workflows"; }
            std::string getUsage() const override { return "workflow [list|create|execute|status] [args...]"; }
        };

        class KOLOSAL_SERVER_API ConfigCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "config"; }
            std::string getDescription() const override { return "Show or modify configuration"; }
            std::string getUsage() const override { return "config [get|set] [key] [value]"; }
        };

        class KOLOSAL_SERVER_API ExitCommand : public Command {
        private:
            CLIInterface* cli_;
        public:
            explicit ExitCommand(CLIInterface* cli) : cli_(cli) {}
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "exit"; }
            std::string getDescription() const override { return "Exit the CLI"; }
            std::string getUsage() const override { return "exit"; }
        };

        class KOLOSAL_SERVER_API AgentExecuteCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "execute"; }
            std::string getDescription() const override { return "Execute an agent function"; }
            std::string getUsage() const override { return "execute <agent_name> <function_name> [args...]"; }
        };

        class KOLOSAL_SERVER_API AgentChatCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "agent-chat"; }
            std::string getDescription() const override { return "Start interactive chat with an agent"; }
            std::string getUsage() const override { return "agent-chat <agent_name> [message]"; }
        };

        class KOLOSAL_SERVER_API OrchestrationCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "orchestrate"; }
            std::string getDescription() const override { return "Manage agent orchestration and collaboration"; }
            std::string getUsage() const override { return "orchestrate [list|create|execute|status] [args...]"; }
        };

        class KOLOSAL_SERVER_API SequentialWorkflowCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "seq-workflow"; }
            std::string getDescription() const override { return "Manage sequential workflows"; }
            std::string getUsage() const override { return "seq-workflow [list|create|execute|status] [args...]"; }
        };

    } // namespace cli
} // namespace kolosal

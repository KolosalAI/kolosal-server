#pragma once

#include "kolosal/cli_interface.hpp"
#include <memory>

namespace kolosal {
    namespace cli {

        // Advanced command that integrates with the inference system
        class KOLOSAL_SERVER_API InferenceCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "infer"; }
            std::string getDescription() const override { return "Run inference with the loaded model"; }
            std::string getUsage() const override { return "infer <prompt> [--model <model>] [--max-tokens <num>] [--temperature <temp>]"; }
        };

        // Command for interactive chat sessions
        class KOLOSAL_SERVER_API InteractiveChatCommand : public Command {
        private:
            void runChatSession(const std::string& modelName = "");
            std::string formatChatHistory(const std::vector<std::pair<std::string, std::string>>& history);
            
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "chat-interactive"; }
            std::string getDescription() const override { return "Start an interactive chat session"; }
            std::string getUsage() const override { return "chat-interactive [model_name]"; }
        };

        // File processing command (like Gemini's @ functionality)
        class KOLOSAL_SERVER_API FileProcessCommand : public Command {
        private:
            std::string readFileContent(const std::string& filepath);
            bool isTextFile(const std::string& filepath);
            
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "process-file"; }
            std::string getDescription() const override { return "Process and analyze file content"; }
            std::string getUsage() const override { return "process-file <filepath> <prompt>"; }
        };

        // Workflow execution command
        class KOLOSAL_SERVER_API WorkflowExecuteCommand : public Command {
        private:
            std::string createWorkflowJson(const std::string& type, const std::vector<std::string>& params);
            
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "run-workflow"; }
            std::string getDescription() const override { return "Execute predefined workflows"; }
            std::string getUsage() const override { return "run-workflow <type> [params...]"; }
        };

        // System information command
        class KOLOSAL_SERVER_API SystemInfoCommand : public Command {
        public:
            CommandResult execute(const std::vector<std::string>& args) override;
            std::string getName() const override { return "sysinfo"; }
            std::string getDescription() const override { return "Show detailed system information"; }
            std::string getUsage() const override { return "sysinfo [--detailed]"; }
        };

    } // namespace cli
} // namespace kolosal

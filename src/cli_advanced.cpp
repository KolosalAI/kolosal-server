#include "kolosal/cli_advanced.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "inference_interface.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#endif

namespace kolosal {
    namespace cli {

        // ANSI color codes (reused from cli_interface.cpp)
        namespace {
            const std::string RESET = "\033[0m";
            const std::string BOLD = "\033[1m";
            const std::string GREEN = "\033[32m";
            const std::string BLUE = "\033[34m";
            const std::string YELLOW = "\033[33m";
            const std::string RED = "\033[31m";
            const std::string CYAN = "\033[36m";
            const std::string MAGENTA = "\033[35m";
            
            std::string trim(const std::string& str) {
                auto start = str.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) return "";
                auto end = str.find_last_not_of(" \t\r\n");
                return str.substr(start, end - start + 1);
            }
        }

        CommandResult InferenceCommand::execute(const std::vector<std::string>& args) {
            if (args.empty()) {
                return {false, "Please provide a prompt for inference"};
            }

            try {
                ServerAPI& server = ServerAPI::instance();
                auto& nodeManager = server.getNodeManager();

                // Parse arguments
                std::string prompt;
                std::string modelName = "default";
                int maxTokens = 128;
                float temperature = 0.7f;

                for (size_t i = 0; i < args.size(); ++i) {
                    if (args[i] == "--model" && i + 1 < args.size()) {
                        modelName = args[++i];
                    } else if (args[i] == "--max-tokens" && i + 1 < args.size()) {
                        maxTokens = std::stoi(args[++i]);
                    } else if (args[i] == "--temperature" && i + 1 < args.size()) {
                        temperature = std::stof(args[++i]);
                    } else if (args[i].substr(0, 2) != "--") {
                        if (!prompt.empty()) prompt += " ";
                        prompt += args[i];
                    }
                }

                if (prompt.empty()) {
                    return {false, "No prompt provided"};
                }

                std::stringstream ss;
                ss << CYAN << "Running Inference" << RESET << "\n";
                ss << "------------------\n";
                ss << "Model: " << GREEN << modelName << RESET << "\n";
                ss << "Prompt: " << YELLOW << prompt << RESET << "\n";
                ss << "Max Tokens: " << CYAN << maxTokens << RESET << "\n";
                ss << "Temperature: " << CYAN << temperature << RESET << "\n\n";

                // TODO: Implement actual inference call
                ss << MAGENTA << "Response:" << RESET << "\n";
                ss << "This is a placeholder response. The actual inference integration\n";
                ss << "will connect to the loaded model and return real AI-generated content.\n";
                ss << "\nInference parameters:\n";
                ss << "- Prompt: " << prompt << "\n";
                ss << "- Model: " << modelName << "\n";
                ss << "- Max tokens: " << maxTokens << "\n";
                ss << "- Temperature: " << temperature << "\n";

                return {true, "", ss.str()};

            } catch (const std::exception& e) {
                return {false, "Inference failed: " + std::string(e.what())};
            }
        }

        CommandResult InteractiveChatCommand::execute(const std::vector<std::string>& args) {
            std::string modelName = args.empty() ? "default" : args[0];
            
            std::cout << BOLD << MAGENTA << "Interactive Chat Session" << RESET << "\n";
            std::cout << "--------------------------\n";
            std::cout << "Model: " << GREEN << modelName << RESET << "\n";
            std::cout << "Type 'exit' to end the session\n";
            std::cout << "Type 'clear' to clear chat history\n\n";

            runChatSession(modelName);
            return {true, ""};
        }

        void InteractiveChatCommand::runChatSession(const std::string& modelName) {
            std::vector<std::pair<std::string, std::string>> chatHistory;
            std::string input;

            while (true) {
                std::cout << BOLD << BLUE << "You: " << RESET;
                std::getline(std::cin, input);
                
                if (std::cin.eof() || input == "exit") {
                    std::cout << GREEN << "Chat session ended." << RESET << "\n";
                    break;
                }
                
                if (input == "clear") {
                    chatHistory.clear();
                    std::cout << YELLOW << "Chat history cleared." << RESET << "\n";
                    continue;
                }
                
                input = trim(input);
                if (input.empty()) continue;

                // Add user message to history
                chatHistory.emplace_back("user", input);

                // TODO: Send to actual inference engine
                std::string response = "This is a simulated AI response to: \"" + input + 
                                     "\"\nThe actual implementation will connect to the inference engine.";

                // Add AI response to history
                chatHistory.emplace_back("assistant", response);

                std::cout << BOLD << GREEN << "AI: " << RESET << response << "\n\n";
            }
        }

        std::string InteractiveChatCommand::formatChatHistory(const std::vector<std::pair<std::string, std::string>>& history) {
            std::stringstream ss;
            for (const auto& message : history) {
                ss << message.first << ": " << message.second << "\n";
            }
            return ss.str();
        }

        CommandResult FileProcessCommand::execute(const std::vector<std::string>& args) {
            if (args.size() < 2) {
                return {false, "Usage: process-file <filepath> <prompt>"};
            }

            std::string filepath = args[0];
            std::string prompt;
            for (size_t i = 1; i < args.size(); ++i) {
                if (i > 1) prompt += " ";
                prompt += args[i];
            }

            try {
                if (!std::filesystem::exists(filepath)) {
                    return {false, "File not found: " + filepath};
                }

                std::string content = readFileContent(filepath);
                if (content.empty()) {
                    return {false, "Could not read file or file is empty: " + filepath};
                }

                std::stringstream ss;
                ss << CYAN << "File Processing" << RESET << "\n";
                ss << "-----------------\n";
                ss << "File: " << GREEN << filepath << RESET << "\n";
                ss << "Size: " << YELLOW << content.length() << " characters" << RESET << "\n";
                ss << "Prompt: " << BLUE << prompt << RESET << "\n\n";

                // Show file preview
                if (content.length() > 500) {
                    ss << MAGENTA << "File Preview (first 500 chars):" << RESET << "\n";
                    ss << content.substr(0, 500) << "...\n\n";
                } else {
                    ss << MAGENTA << "File Content:" << RESET << "\n";
                    ss << content << "\n\n";
                }

                // TODO: Send file content + prompt to inference engine
                ss << YELLOW << "AI Analysis:" << RESET << "\n";
                ss << "This would analyze the file content with the given prompt.\n";
                ss << "The implementation will combine file content with user prompt\n";
                ss << "and send to the inference engine for processing.\n";

                return {true, "", ss.str()};

            } catch (const std::exception& e) {
                return {false, "File processing failed: " + std::string(e.what())};
            }
        }

        std::string FileProcessCommand::readFileContent(const std::string& filepath) {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                return "";
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        bool FileProcessCommand::isTextFile(const std::string& filepath) {
            std::string ext = std::filesystem::path(filepath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            std::vector<std::string> textExts = {
                ".txt", ".md", ".cpp", ".hpp", ".h", ".c", ".py", ".js", ".ts",
                ".html", ".css", ".json", ".xml", ".yaml", ".yml", ".log"
            };
            
            return std::find(textExts.begin(), textExts.end(), ext) != textExts.end();
        }

        CommandResult WorkflowExecuteCommand::execute(const std::vector<std::string>& args) {
            if (args.empty()) {
                return {false, "Please specify a workflow type (content, code, analysis)"};
            }

            std::string workflowType = args[0];
            std::vector<std::string> params(args.begin() + 1, args.end());

            try {
                ServerAPI& server = ServerAPI::instance();

                std::stringstream ss;
                ss << CYAN << "🔄 Workflow Execution" << RESET << "\n";
                ss << "────────────────────\n";
                ss << "Type: " << GREEN << workflowType << RESET << "\n";

                if (workflowType == "content") {
                    if (params.empty()) {
                        return {false, "Content workflow requires a topic parameter"};
                    }
                    ss << "Topic: " << YELLOW << params[0] << RESET << "\n";
                    ss << "Target Audience: " << CYAN << (params.size() > 1 ? params[1] : "general") << RESET << "\n\n";
                    
                    ss << MAGENTA << "Executing Content Creation Workflow:" << RESET << "\n";
                    ss << "1. [Search] Research phase...\n";
                    ss << "2. [Write] Writing phase...\n";
                    ss << "3. [Review] Review phase...\n\n";
                    
                    ss << GREEN << "Content creation workflow would execute here with the simplified workflow system." << RESET << "\n";

                } else if (workflowType == "code") {
                    if (params.empty()) {
                        return {false, "Code workflow requires a description parameter"};
                    }
                    ss << "Description: " << YELLOW;
                    for (size_t i = 0; i < params.size(); ++i) {
                        if (i > 0) ss << " ";
                        ss << params[i];
                    }
                    ss << RESET << "\n\n";
                    
                    ss << MAGENTA << "Executing Code Development Workflow:" << RESET << "\n";
                    ss << "1. [Plan] Analysis phase...\n";
                    ss << "2. [Code] Development phase...\n";
                    ss << "3. [Test] Testing phase...\n\n";
                    
                    ss << GREEN << "Code development workflow would execute here." << RESET << "\n";

                } else if (workflowType == "analysis") {
                    if (params.empty()) {
                        return {false, "Analysis workflow requires data source parameter"};
                    }
                    ss << "Data Source: " << YELLOW << params[0] << RESET << "\n\n";
                    
                    ss << MAGENTA << "Executing Data Analysis Workflow:" << RESET << "\n";
                    ss << "1. [Data] Data ingestion...\n";
                    ss << "2. [Analyze] Pattern analysis...\n";
                    ss << "3. [Report] Insight generation...\n\n";
                    
                    ss << GREEN << "Data analysis workflow would execute here." << RESET << "\n";

                } else {
                    return {false, "Unknown workflow type. Available: content, code, analysis"};
                }

                return {true, "", ss.str()};

            } catch (const std::exception& e) {
                return {false, "Workflow execution failed: " + std::string(e.what())};
            }
        }

        CommandResult SystemInfoCommand::execute(const std::vector<std::string>& args) {
            bool detailed = !args.empty() && args[0] == "--detailed";

            std::stringstream ss;
            ss << BOLD << CYAN << "System Information" << RESET << "\n";
            ss << "---------------------\n";

            try {
                ServerAPI& server = ServerAPI::instance();

                // Basic system info
                ss << GREEN << "Server Status:" << RESET << "\n";
                ss << "  Version: " << YELLOW << "1.0.0" << RESET << "\n";
                ss << "  Build: " << YELLOW << __DATE__ << " " << __TIME__ << RESET << "\n";

#ifdef _WIN32
                SYSTEM_INFO sysInfo;
                GetSystemInfo(&sysInfo);
                ss << "  Platform: " << YELLOW << "Windows" << RESET << "\n";
                ss << "  Processors: " << YELLOW << sysInfo.dwNumberOfProcessors << RESET << "\n";
#else
                struct utsname unameData;
                if (uname(&unameData) == 0) {
                    ss << "  Platform: " << YELLOW << unameData.sysname << " " << unameData.release << RESET << "\n";
                    ss << "  Architecture: " << YELLOW << unameData.machine << RESET << "\n";
                }
                
                long processors = sysconf(_SC_NPROCESSORS_ONLN);
                if (processors > 0) {
                    ss << "  Processors: " << YELLOW << processors << RESET << "\n";
                }
#endif

                // Memory info
                if (detailed) {
#ifdef _WIN32
                    MEMORYSTATUSEX memInfo;
                    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
                    if (GlobalMemoryStatusEx(&memInfo)) {
                        ss << "\n" << GREEN << "Memory Information:" << RESET << "\n";
                        ss << "  Total Physical: " << YELLOW << (memInfo.ullTotalPhys / (1024 * 1024)) << " MB" << RESET << "\n";
                        ss << "  Available Physical: " << YELLOW << (memInfo.ullAvailPhys / (1024 * 1024)) << " MB" << RESET << "\n";
                        ss << "  Memory Load: " << YELLOW << memInfo.dwMemoryLoad << "%" << RESET << "\n";
                    }
#else
                    struct sysinfo sysInfo;
                    if (sysinfo(&sysInfo) == 0) {
                        ss << "\n" << GREEN << "Memory Information:" << RESET << "\n";
                        ss << "  Total RAM: " << YELLOW << (sysInfo.totalram * sysInfo.mem_unit / (1024 * 1024)) << " MB" << RESET << "\n";
                        ss << "  Free RAM: " << YELLOW << (sysInfo.freeram * sysInfo.mem_unit / (1024 * 1024)) << " MB" << RESET << "\n";
                        ss << "  Uptime: " << YELLOW << (sysInfo.uptime / 3600) << " hours" << RESET << "\n";
                    }
#endif
                }

                // Server components status
                ss << "\n" << GREEN << "Components Status:" << RESET << "\n";

                try {
                    auto& nodeManager = server.getNodeManager();
                    ss << "  Node Manager: " << GREEN << "✓ Active" << RESET << "\n";
                } catch (...) {
                    ss << "  Node Manager: " << RED << "✗ Error" << RESET << "\n";
                }

                try {
                    auto& agentManager = server.getAgentManager();
                    ss << "  Agent Manager: " << GREEN << "✓ Active" << RESET << "\n";
                } catch (...) {
                    ss << "  Agent Manager: " << RED << "✗ Error" << RESET << "\n";
                }

                try {
                    auto& autoSetup = server.getAutoSetupManager();
                    ss << "  Auto Setup: " << GREEN << "✓ Active" << RESET << "\n";
                } catch (...) {
                    ss << "  Auto Setup: " << RED << "✗ Error" << RESET << "\n";
                }

                // File system info
                if (detailed) {
                    ss << "\n" << GREEN << "File System:" << RESET << "\n";
                    
                    std::filesystem::path currentPath = std::filesystem::current_path();
                    ss << "  Working Directory: " << YELLOW << currentPath.string() << RESET << "\n";
                    
                    if (std::filesystem::exists("downloads")) {
                        auto downloadSize = 0;
                        for (const auto& entry : std::filesystem::directory_iterator("downloads")) {
                            if (entry.is_regular_file()) {
                                downloadSize += std::filesystem::file_size(entry);
                            }
                        }
                        ss << "  Downloads Size: " << YELLOW << (downloadSize / (1024 * 1024)) << " MB" << RESET << "\n";
                    }
                    
                    if (std::filesystem::exists("config")) {
                        ss << "  Config Directory: " << GREEN << "✓ Present" << RESET << "\n";
                    }
                }

                return {true, "", ss.str()};

            } catch (const std::exception& e) {
                return {false, "Failed to get system information: " + std::string(e.what())};
            }
        }

    } // namespace cli
} // namespace kolosal

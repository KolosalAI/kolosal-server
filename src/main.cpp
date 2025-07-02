#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include "kolosal_server.hpp"

#ifdef KOLOSAL_CLI_ENABLED
#include "kolosal/cli_interface.hpp"
#endif

using namespace kolosal;

// Global flag for graceful shutdown
std::atomic<bool> keep_running{true};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    keep_running = false;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "Options:\n"
              << "  -p, --port PORT    Server port (default: 8080)\n"
#ifdef KOLOSAL_CLI_ENABLED
              << "  -c, --cli          Start in CLI mode instead of server mode\n"
#endif
              << "  -h, --help         Show this help message\n"
              << "  -v, --version      Show version information\n"
              << std::endl;
}

void print_version() {
    std::cout << "Kolosal Server v1.0.0\n"
              << "A high-performance HTTP server for AI inference\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::string port = "8080";
    bool cli_mode = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--version") {
            print_version();
            return 0;
        }
#ifdef KOLOSAL_CLI_ENABLED
        else if (arg == "-c" || arg == "--cli") {
            cli_mode = true;
        }
#endif
        else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = argv[++i];
        }
        else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate port number
    try {
        int port_num = std::stoi(port);
        if (port_num < 1 || port_num > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid port number: " << port << " (" << e.what() << ")" << std::endl;
        return 1;
    }
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifdef _WIN32
    std::signal(SIGBREAK, signal_handler);
#endif

    // Check if CLI mode is requested
    if (cli_mode) {
#ifdef KOLOSAL_CLI_ENABLED
        std::cout << "Starting Kolosal CLI..." << std::endl;
        
        // Initialize the server first for CLI to access
        ServerAPI& server = ServerAPI::instance();
        if (!server.init(port)) {
            std::cerr << "Failed to initialize server for CLI mode on port " << port << std::endl;
            return 1;
        }
        
        // Start CLI interface
        kolosal::cli::CLIInterface cli;
        cli.start();
        
        // Cleanup
        server.shutdown();
        return 0;
#else
        std::cout << "CLI mode is not enabled in this build." << std::endl;
        std::cout << "Please recompile with -DENABLE_CLI=ON to enable CLI support." << std::endl;
        return 1;
#endif
    }
    
    std::cout << "Starting Kolosal Server v1.0.0..." << std::endl;
    std::cout << "Port: " << port << std::endl;
    
    // Initialize the server
    ServerAPI& server = ServerAPI::instance();
    
    if (!server.init(port)) {
        std::cerr << "Failed to initialize server on port " << port << std::endl;
        return 1;
    }
      std::cout << "Server started successfully!" << std::endl;
    std::cout << "\n🎉 KOLOSAL SERVER WITH AUTO-SETUP ENABLED!" << std::endl;
    std::cout << "✅ Automatic model downloading and engine setup" << std::endl;
    std::cout << "✅ Automatic agent discovery and UUID mapping" << std::endl;
    std::cout << "✅ Simplified workflow creation with agent names" << std::endl;
    
    std::cout << "\nCore Endpoints:" << std::endl;
    std::cout << "  GET  /health                 - Health status" << std::endl;
    std::cout << "  GET  /models                 - List available models" << std::endl;
    std::cout << "  POST /v1/chat/completions    - Chat completions (OpenAI compatible)" << std::endl;
    std::cout << "  POST /v1/completions         - Text completions (OpenAI compatible)" << std::endl;
    
    std::cout << "\nEngine Management:" << std::endl;
    std::cout << "  GET  /engines                - List engines" << std::endl;
    std::cout << "  POST /engines                - Add new engine" << std::endl;
    std::cout << "  GET  /engines/{id}/status    - Engine status" << std::endl;
    std::cout << "  DELETE /engines/{id}         - Remove engine" << std::endl;
    
    std::cout << "\nAgent System:" << std::endl;
    std::cout << "  GET  /v1/agents              - List all agents" << std::endl;
    std::cout << "  GET  /v1/agents/{id}         - Get agent details" << std::endl;
    std::cout << "  POST /v1/agents              - Create new agent" << std::endl;
    std::cout << "  POST /v1/agents/{id}/execute - Execute agent function" << std::endl;
    std::cout << "  GET  /v1/agents/system/status - Agent system status" << std::endl;
    
    std::cout << "\nWorkflow System (Auto-Setup Enabled):" << std::endl;
    std::cout << "  GET  /api/v1/sequential-workflows - List sequential workflows" << std::endl;
    std::cout << "  POST /api/v1/sequential-workflows - Create workflow (auto agent mapping!)" << std::endl;
    std::cout << "  GET  /api/v1/sequential-workflows/{id} - Get workflow details" << std::endl;
    std::cout << "  POST /api/v1/sequential-workflows/{id}/execute - Execute workflow" << std::endl;
    std::cout << "  POST /api/v1/sequential-workflows/{id}/execute-async - Execute workflow async" << std::endl;
    std::cout << "  GET  /api/v1/sequential-workflows/{id}/status - Get workflow status" << std::endl;
    std::cout << "  GET  /api/v1/sequential-workflows/{id}/result - Get workflow result" << std::endl;
    std::cout << "  DELETE /api/v1/sequential-workflows/{id} - Delete workflow" << std::endl;
    
    std::cout << "\n🔧 Auto-Setup System:" << std::endl;
    std::cout << "  GET  /api/v1/auto-setup              - Get setup status" << std::endl;
    std::cout << "  POST /api/v1/auto-setup              - Trigger manual setup" << std::endl;
    std::cout << "  GET  /api/v1/auto-setup/agent-mappings - Get agent name mappings" << std::endl;
    std::cout << "  GET  /api/v1/auto-setup/engine-status  - Get engine readiness" << std::endl;
    std::cout << "  POST /api/v1/auto-setup/validate-workflow - Validate workflow JSON" << std::endl;
    
    std::cout << "\n💡 NEW SIMPLIFIED WORKFLOW USAGE:" << std::endl;
    std::cout << "   - Use agent NAMES instead of UUIDs (e.g. 'research_assistant')" << std::endl;
    std::cout << "   - Server automatically maps names to UUIDs" << std::endl;
    std::cout << "   - Models download automatically if missing" << std::endl;
    std::cout << "   - Default engine created automatically" << std::endl;
    
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
    
    // Main server loop
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Shutting down server..." << std::endl;
    server.shutdown();
    std::cout << "Server stopped." << std::endl;
    
    return 0;
}

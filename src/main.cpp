#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include "kolosal_server.hpp"

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
        std::cerr << "Error: Invalid port number: " << port << std::endl;
        return 1;
    }
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifdef _WIN32
    std::signal(SIGBREAK, signal_handler);
#endif
    
    std::cout << "Starting Kolosal Server v1.0.0..." << std::endl;
    std::cout << "Port: " << port << std::endl;
    
    // Initialize the server
    ServerAPI& server = ServerAPI::instance();
    
    if (!server.init(port)) {
        std::cerr << "Failed to initialize server on port " << port << std::endl;
        return 1;
    }
    
    std::cout << "Server started successfully!" << std::endl;
    std::cout << "Available endpoints:" << std::endl;
    std::cout << "  GET  /health                 - Health status" << std::endl;
    std::cout << "  GET  /models                 - List available models" << std::endl;
    std::cout << "  POST /v1/chat/completions    - Chat completions (OpenAI compatible)" << std::endl;
    std::cout << "  POST /v1/completions         - Text completions (OpenAI compatible)" << std::endl;
    std::cout << "  GET  /engines                - List engines" << std::endl;
    std::cout << "  POST /engines                - Add new engine" << std::endl;
    std::cout << "  GET  /engines/{id}/status    - Engine status" << std::endl;
    std::cout << "  DELETE /engines/{id}         - Remove engine" << std::endl;
    std::cout << "  POST /parse_pdf              - Parse PDF document" << std::endl;
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

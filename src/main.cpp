#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <vector>
#include <filesystem>
#include <cstring>
#ifdef _WIN32
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <wininet.h>
#include <io.h>
#include <fcntl.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
#define write _write
#define STDERR_FILENO _fileno(stderr)
#else
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "kolosal_server.hpp"
#include "kolosal/server_config.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/auth/auth_middleware.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/download_utils.hpp"
#include "kolosal/agents/multi_agent_system.hpp"
#include "kolosal/agents/agent_orchestrator.hpp"

#ifdef KOLOSAL_CLI_ENABLED
#include "kolosal/cli_interface.hpp"
#endif

using namespace kolosal;

// Global flag for graceful shutdown
std::atomic<bool> keep_running{true};

// Function to get local IP addresses
std::vector<std::string> getLocalIPAddresses()
{
    std::vector<std::string> addresses;

#ifdef _WIN32
    // Windows implementation
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return addresses;
    }

    ULONG bufferSize = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &bufferSize);

    if (bufferSize > 0)
    {
        auto buffer = std::make_unique<char[]>(bufferSize);
        PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.get());

        if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &bufferSize) == NO_ERROR)
        {
            for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter != nullptr; adapter = adapter->Next)
            {
                if (adapter->OperStatus == IfOperStatusUp)
                {
                    for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress;
                         unicast != nullptr; unicast = unicast->Next)
                    {

                        wchar_t ipStr[INET6_ADDRSTRLEN];
                        DWORD ipStrLen = INET6_ADDRSTRLEN;

                        if (WSAAddressToStringW(unicast->Address.lpSockaddr,
                                                unicast->Address.iSockaddrLength,
                                                nullptr, ipStr, &ipStrLen) == 0)
                        {
                            // Convert wide string to narrow string
                            int len = WideCharToMultiByte(CP_UTF8, 0, ipStr, -1, nullptr, 0, nullptr, nullptr);
                            if (len > 0) {
                                std::vector<char> buffer(len);
                                WideCharToMultiByte(CP_UTF8, 0, ipStr, -1, buffer.data(), len, nullptr, nullptr);
                                std::string ip(buffer.data());
                                
                                // Filter out loopback, link-local, and IPv6 addresses for simplicity
                                if (ip != "127.0.0.1" && ip.find("169.254.") != 0 && ip.find(":") == std::string::npos)
                                {
                                    addresses.push_back(ip);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    WSACleanup();
#else
    // Unix/Linux implementation
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == 0)
    {
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr)
                continue;

            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sa->sin_addr, ip, INET_ADDRSTRLEN);

                std::string ipStr(ip);
                // Filter out loopback and link-local addresses
                if (ipStr != "127.0.0.1" && ipStr.find("169.254.") != 0)
                {
                    addresses.push_back(ipStr);
                }
            }
        }
        freeifaddrs(ifaddr);
    }
#endif
    return addresses;
}

// Function to get public IP address using external services
std::string getPublicIPAddress()
{
    // We'll use a simple HTTP request to get the public IP
    // This is a basic implementation - in production you might want to use multiple services as fallback
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("KolosalServer/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInternet)
        return "";

    HINTERNET hConnect = InternetOpenUrlA(hInternet, "http://httpbin.org/ip", nullptr, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect)
    {
        InternetCloseHandle(hInternet);
        return "";
    }

    char buffer[1024];
    DWORD bytesRead;
    std::string response;

    while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    // Parse JSON response to extract IP
    size_t start = response.find("\"origin\": \"");
    if (start != std::string::npos)
    {
        start += 11; // Length of "\"origin\": \""
        size_t end = response.find("\"", start);
        if (end != std::string::npos)
        {
            return response.substr(start, end - start);
        }
    }
#else
    // For Linux/Mac, we can use curl or similar
    // This is a simplified implementation
    FILE *pipe = popen("curl -s http://httpbin.org/ip | grep -o '\"origin\": \"[^\"]*' | cut -d'\"' -f4", "r");
    if (pipe)
    {
        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            result += buffer;
        }
        pclose(pipe);

        // Remove newline
        if (!result.empty() && result.back() == '\n')
        {
            result.pop_back();
        }
        return result;
    }
#endif

    return "";
}

// Function to attempt UPnP port forwarding
bool configureUPnPPortForwarding(const std::string &port)
{
    // This is a simplified UPnP implementation
    // In a production environment, you'd want to use a proper UPnP library

    std::cout << "\nAttempting to configure UPnP port forwarding for port " << port << "..." << std::endl;

#ifdef _WIN32
    // Windows UPnP using COM
    // This is a basic implementation - you might want to use a more robust UPnP library
    std::cout << "   UPnP configuration on Windows requires additional setup." << std::endl;
    std::cout << "   Please manually configure port forwarding in your router for port " << port << std::endl;
    return false;
#else
    // Try using upnpc if available
    std::string command = "upnpc -a " + getLocalIPAddresses()[0] + " " + port + " " + port + " TCP";
    int result = system(command.c_str());
    if (result == 0)
    {
        std::cout << "   UPnP port forwarding configured successfully!" << std::endl;
        return true;
    }
    else
    {
        std::cout << "   UPnP port forwarding failed. Please manually configure your router." << std::endl;
        return false;
    }
#endif
}

// Signal handler for graceful shutdown
void signal_handler(int signal)
{
    // Use async-signal-safe functions only
    const char* msg = "\nReceived shutdown signal, shutting down gracefully...\n";
    write(STDERR_FILENO, msg, strlen(msg));
    keep_running.store(false, std::memory_order_release);
}

void print_usage(const char *program_name)
{
    ServerConfig config;
    config.printHelp();
}

void print_version()
{
    ServerConfig config;
    config.printVersion();
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Set console to UTF-8 for proper character display
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    // Load configuration from command line arguments
    ServerConfig config;
    if (!config.loadFromArgs(argc, argv))
    {
        // If help or version was shown, exit successfully
        if (config.helpOrVersionShown) {
            return 0;
        }
        // Otherwise, it was an error - validate and return appropriate code
        return config.validate() ? 0 : 1;
    }

    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifdef _WIN32
    std::signal(SIGBREAK, signal_handler);
#endif

    // Print startup banner
    std::cout << "Starting Kolosal Server v1.0.0..." << std::endl;
    config.printSummary();
    
    // Set the singleton instance with our loaded configuration
    // This is crucial for NodeManager to access the inference engines
    ServerConfig::setInstance(config);
    
    // Configure logger based on loaded config
    auto& logger = ServerLogger::instance();
    
    // Convert string log level to enum
    LogLevel logLevel = LogLevel::SERVER_INFO; // default
    if (config.logLevel == "ERROR") {
        logLevel = LogLevel::SERVER_ERROR;
    } else if (config.logLevel == "WARNING" || config.logLevel == "WARN") {
        logLevel = LogLevel::SERVER_WARNING;
    } else if (config.logLevel == "INFO") {
        logLevel = LogLevel::SERVER_INFO;
    } else if (config.logLevel == "DEBUG") {
        logLevel = LogLevel::SERVER_DEBUG;
    }
    
    logger.setLevel(logLevel);
    logger.setQuietMode(config.quietMode);
    logger.setShowRequestDetails(config.showRequestDetails);
    
    // Set log file if specified
    if (!config.logFile.empty()) {
        if (!logger.setLogFile(config.logFile)) {
            std::cerr << "Warning: Failed to open log file: " << config.logFile << std::endl;
        }
    }
    
    ServerLogger::logInfo("Logger configured - Level: %s, Quiet: %s, Details: %s", 
                         config.logLevel.c_str(),
                         config.quietMode ? "true" : "false",
                         config.showRequestDetails ? "true" : "false");

    // Initialize the server
    ServerAPI &server = ServerAPI::instance();

    // Initialize agent system from main configuration
    std::shared_ptr<kolosal::agents::YAMLConfigurableAgentManager> agentManager;
    std::shared_ptr<kolosal::agents::AgentOrchestrator> agentOrchestrator;
    
    // Check if agent system configuration exists in the main config
    if (!config.agentSystem.agents.empty() || !config.agentSystem.functions.empty()) {
        try {
            ServerLogger::logInfo("Initializing agent system from main configuration");
            
            agentManager = std::make_shared<kolosal::agents::YAMLConfigurableAgentManager>();
            std::cout << "DEBUG: Agent manager created" << std::endl;
            std::cout.flush();
            
            std::cout << "DEBUG: About to configure agent system from ServerConfig" << std::endl;
            std::cout.flush();
            
            bool configLoaded = false;
            try {
                // Load configuration directly from the ServerConfig's agent system
                configLoaded = agentManager->load_configuration(config.agentSystem);
                std::cout << "DEBUG: load_configuration(SystemConfig) returned: " << (configLoaded ? "true" : "false") << std::endl;
                std::cout.flush();
            } catch (const std::exception& e) {
                std::cout << "DEBUG: Exception in load_configuration(SystemConfig): " << e.what() << std::endl;
                std::cout.flush();
                ServerLogger::logError("Exception loading agent configuration: %s", e.what());
                std::cerr << "Error loading agent configuration: " << e.what() << std::endl;
                return 1;
            } catch (...) {
                std::cout << "DEBUG: Unknown exception in load_configuration(SystemConfig)" << std::endl;
                std::cout.flush();
                ServerLogger::logError("Unknown exception loading agent configuration");
                std::cerr << "Unknown error loading agent configuration" << std::endl;
                return 1;
            }
            
            if (configLoaded) {
                std::cout << "DEBUG: Agent configuration loaded successfully" << std::endl;
                std::cout.flush();
                
                ServerLogger::logInfo("Agent configuration loaded successfully, starting agent manager...");
                try {
                    ServerLogger::logInfo("DEBUG: About to call agentManager->start()");
                    std::cout << "DEBUG: About to call agentManager->start()" << std::endl;
                    std::cout.flush();
                    
                    agentManager->start();
                    
                    std::cout << "DEBUG: agentManager->start() completed successfully" << std::endl;
                    std::cout.flush();
                    
                    ServerLogger::logInfo("DEBUG: agentManager->start() completed successfully");
                    ServerLogger::logInfo("Agent manager started successfully");
                } catch (const std::exception& e) {
                    ServerLogger::logError("Exception starting agent manager: %s", e.what());
                    std::cerr << "Error starting agent manager: " << e.what() << std::endl;
                    std::cout << "DEBUG: Exception in agentManager->start(): " << e.what() << std::endl;
                    std::cout.flush();
                    return 1;
                } catch (...) {
                    ServerLogger::logError("Unknown exception starting agent manager");
                    std::cerr << "Unknown error starting agent manager" << std::endl;
                    std::cout << "DEBUG: Unknown exception in agentManager->start()" << std::endl;
                    std::cout.flush();
                    return 1;
                }
                
                // Initialize orchestrator for advanced workflows
                ServerLogger::logInfo("Initializing agent orchestrator...");
                std::cout << "DEBUG: Initializing agent orchestrator..." << std::endl;
                std::cout.flush();
                
                try {
                    agentOrchestrator = std::make_shared<kolosal::agents::AgentOrchestrator>(agentManager);
                    std::cout << "DEBUG: Agent orchestrator created" << std::endl;
                    std::cout.flush();
                    
                    agentOrchestrator->start();
                    std::cout << "DEBUG: Agent orchestrator started" << std::endl;
                    std::cout.flush();
                    
                    ServerLogger::logInfo("Agent orchestrator started successfully");
                } catch (const std::exception& e) {
                    ServerLogger::logError("Exception starting agent orchestrator: %s", e.what());
                    std::cerr << "Error starting agent orchestrator: " << e.what() << std::endl;
                    return 1;
                }
                
                ServerLogger::logInfo("Agent system initialized successfully");
            } else {
                ServerLogger::logError("Failed to load agent configuration - agent system disabled");
                std::cerr << "Warning: Failed to load agent configuration - continuing without agent system" << std::endl;
                // Continue without agent system instead of exiting
            }
        } catch (const std::exception& e) {
            ServerLogger::logError("Failed to initialize agent system: %s", e.what());
            std::cerr << "Warning: Agent system initialization failed: " << e.what() << std::endl;
            std::cerr << "Continuing without agent system..." << std::endl;
            // Continue without agent system
        }
    } else {
        ServerLogger::logInfo("No agent system configuration found in main config - continuing without agent system");
        std::cout << "No agent system configuration found - continuing without agent system" << std::endl;
        std::cout.flush();
    }
    
    ServerLogger::logInfo("Agent system initialization complete, proceeding with server setup...");
    std::cout << "Agent system initialization complete, proceeding with server setup..." << std::endl;
    std::cout.flush();

    // Determine the actual host to bind to based on public access setting
    std::string bindHost = config.host;
    if (!config.allowPublicAccess && config.host == "0.0.0.0")
    {
        // If public access is disabled and host is set to bind all interfaces,
        // change it to localhost only for security
        bindHost = "127.0.0.1";
        std::cout << "Public access disabled - binding to localhost only (127.0.0.1)" << std::endl;
    }
    else if (config.allowPublicAccess && config.host == "127.0.0.1")
    {
        // If public access is enabled but host is localhost, warn user
        std::cout << "Warning: Public access enabled but host is set to 127.0.0.1 (localhost only)" << std::endl;
        std::cout << "Server will only be accessible from this machine" << std::endl;
    }

    ServerLogger::logInfo("Attempting to initialize server on %s:%s", bindHost.c_str(), config.port.c_str());
    std::cout << "Attempting to initialize server on " << bindHost << ":" << config.port << std::endl;
    std::cout.flush();
    
    if (!server.init(config.port, bindHost, config.idleTimeout, config))
    {
        std::cerr << "Failed to initialize server on " << bindHost << ":" << config.port << std::endl;
        ServerLogger::logError("Server initialization failed on %s:%s", bindHost.c_str(), config.port.c_str());
        return 1;
    }
    
    ServerLogger::logInfo("Server initialized successfully on %s:%s", bindHost.c_str(), config.port.c_str());
    std::cout << "Server initialized successfully on " << bindHost << ":" << config.port << std::endl;
    std::cout.flush(); // Configure authentication if enabled
    if (config.auth.enableAuth)
    {
        try
        {
            auto &authMiddleware = server.getAuthMiddleware();

            // Update rate limiter configuration
            authMiddleware.updateRateLimiterConfig(config.auth.rateLimiter);

            // Update CORS configuration
            authMiddleware.updateCorsConfig(config.auth.cors);

            // Configure API key authentication
            kolosal::auth::AuthMiddleware::ApiKeyConfig apiKeyConfig;
            apiKeyConfig.enabled = config.auth.enableAuth;
            apiKeyConfig.required = config.auth.requireApiKey;
            apiKeyConfig.headerName = config.auth.apiKeyHeader;

            // Add all configured API keys
            for (const auto &key : config.auth.allowedApiKeys)
            {
                apiKeyConfig.validKeys.insert(key);
            }

            authMiddleware.updateApiKeyConfig(apiKeyConfig);

            ServerLogger::logInfo("Authentication configured - Rate Limit: %s, CORS: %s, API Keys: %s (%zu keys)",
                                  config.auth.rateLimiter.enabled ? "enabled" : "disabled",
                                  config.auth.cors.enabled ? "enabled" : "disabled",
                                  config.auth.requireApiKey ? "required" : "optional",
                                  config.auth.allowedApiKeys.size());
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to configure authentication: " << e.what() << std::endl;
            return 1;
        }
    }

    // Enable metrics if configured
    if (config.enableMetrics)
    {
        try
        {
            server.enableMetrics();
            ServerLogger::logInfo("System metrics monitoring enabled");
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to enable metrics: " << e.what() << std::endl;
            return 1;
        }
    } // Enable internet search if configured
    if (config.search.enabled)
    {
        try
        {
            server.enableSearch(config.search);
            ServerLogger::logInfo("Internet search endpoint enabled");
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to enable internet search: " << e.what() << std::endl;
            return 1;
        }
    }

    // Register agent manager with server for route access (after server init)
    if (agentManager) {
        ServerLogger::logInfo("Registering agent manager with server...");
        try {
            server.setAgentManager(agentManager);
            ServerLogger::logInfo("Agent manager set successfully");
            
            if (agentOrchestrator) {
                ServerLogger::logInfo("Setting agent orchestrator...");
                std::cout << "DEBUG: About to call setAgentOrchestrator" << std::endl;
                std::cout.flush();
                
                server.setAgentOrchestrator(agentOrchestrator);
                
                std::cout << "DEBUG: setAgentOrchestrator completed successfully" << std::endl;
                std::cout.flush();
                ServerLogger::logInfo("Agent orchestrator set successfully");
            } else {
                ServerLogger::logWarning("agentOrchestrator is null - orchestration routes will not be available");
                std::cout << "WARNING: agentOrchestrator is null" << std::endl;
            }
            ServerLogger::logInfo("Agent manager registered with server");
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception registering agent manager with server: %s", e.what());
            std::cerr << "Error registering agent manager with server: " << e.what() << std::endl;
            std::cout << "DEBUG: Exception in agent registration: " << e.what() << std::endl;
            return 1;
        }
    } else {
        ServerLogger::logWarning("agentManager is null - agent routes will not be available");
        std::cout << "WARNING: agentManager is null" << std::endl;
    }
    
    // Load models if specified
    if (!config.models.empty())
    {
        auto &downloadManager = DownloadManager::getInstance();

        int successfulModels = 0;
        int failedModels = 0;
        int asyncDownloads = 0;

        for (const auto &modelConfig : config.models)
        {
            std::cout << "Configuring model '" << modelConfig.id << "'..." << std::endl;            // Use DownloadManager to handle both URLs and local files consistently
            bool success = downloadManager.loadModelAtStartup(modelConfig.id,
                                                              modelConfig.path,
                                                              modelConfig.type,
                                                              modelConfig.loadParams,
                                                              modelConfig.mainGpuId,
                                                              modelConfig.loadImmediately,
                                                              modelConfig.inferenceEngine);

            if (success)
            {
                // Check if this was a URL that started an async download
                if (kolosal::is_valid_url(modelConfig.path) && !std::filesystem::exists(kolosal::generate_download_path(modelConfig.path, "./models")))
                {
                    std::cout << "✓ Model '" << modelConfig.id << "' download started (async)" << std::endl;
                    ServerLogger::logInfo("Model '%s' download started from URL: %s", modelConfig.id.c_str(), modelConfig.path.c_str());
                    asyncDownloads++;
                }
                else if (modelConfig.loadImmediately)
                {
                    std::cout << "✓ Model '" << modelConfig.id << "' loaded successfully" << std::endl;
                    ServerLogger::logInfo("Model '%s' loaded successfully", modelConfig.id.c_str());
                }
                else
                {
                    std::cout << "✓ Model '" << modelConfig.id << "' registered for lazy loading" << std::endl;
                    ServerLogger::logInfo("Model '%s' registered for lazy loading", modelConfig.id.c_str());
                }
                successfulModels++;
            }
            else
            {
                std::cerr << "✗ Failed to configure model '" << modelConfig.id << "' - skipping" << std::endl;
                ServerLogger::logWarning("Failed to configure model '%s' from %s - continuing with other models",
                                         modelConfig.id.c_str(), modelConfig.path.c_str());
                failedModels++;
            }
        }

        // Log summary of model loading
        if (successfulModels > 0)
        {
            std::cout << "\n✓ Successfully configured " << successfulModels << " model(s)";
            if (asyncDownloads > 0)
            {
                std::cout << " (" << asyncDownloads << " downloading asynchronously)";
            }
            std::cout << std::endl;
        }
        if (failedModels > 0)
        {
            std::cout << "⚠ " << failedModels << " model(s) failed to configure" << std::endl;
        }
        if (asyncDownloads > 0)
        {
            std::cout << "\n📊 Monitor download progress using: GET /download-progress/{model-id}" << std::endl;
            std::cout << "📊 View all downloads using: GET /downloads" << std::endl;
        }

        if (failedModels > 0)
        {
            ServerLogger::logWarning("Server started with %d failed model(s) out of %d total",
                                     failedModels, (int)config.models.size());
        }
    }
    std::cout << "\nServer started successfully!" << std::endl;
    
    // Finalize route registration (add catch-all routes)
    server.finalizeRoutes();

    // Give the server thread a moment to fully start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Display appropriate server URLs based on configuration
    if (config.allowPublicAccess && (bindHost == "0.0.0.0" || bindHost == "::"))
    {
        std::cout << "Server is accessible from:" << std::endl;
        std::cout << "  Local:    http://127.0.0.1:" << config.port << std::endl;
        std::cout << "  Network:  http://<your-ip>:" << config.port << std::endl;
        std::cout << "  Note: Replace <your-ip> with your actual IP address" << std::endl;
    }
    else if (bindHost == "127.0.0.1" || bindHost == "localhost")
    {
        std::cout << "Server URL (localhost only): http://127.0.0.1:" << config.port << std::endl;
        if (config.allowPublicAccess)
        {
            std::cout << "Warning: Public access is enabled but server is bound to localhost only" << std::endl;
        }
    }
    else
    {
        std::cout << "Server URL: http://" << bindHost << ":" << config.port << std::endl;
    }
    if (config.allowPublicAccess)
    {
        std::cout << "\n🌐 Public access is ENABLED - server accessible from other devices" << std::endl;
        std::cout << "   Make sure your firewall allows connections on port " << config.port << std::endl;

        // Get and display local IP addresses
        auto ipAddresses = getLocalIPAddresses();
        if (!ipAddresses.empty())
        {
            std::cout << "\n📍 Server accessible at the following addresses:" << std::endl;
            std::cout << "   • http://localhost:" << config.port << " (local machine only)" << std::endl;
            for (const auto &ip : ipAddresses)
            {
                std::cout << "   • http://" << ip << ":" << config.port << " (network access)" << std::endl;
            }
        }
        else
        {
            std::cout << "\n📍 Server accessible at:" << std::endl;
            std::cout << "   • http://localhost:" << config.port << " (local machine)" << std::endl;
            std::cout << "   • http://<your-ip-address>:" << config.port << " (network access)" << std::endl;
            std::cout << "   Note: Could not automatically detect IP address. Use 'ipconfig' (Windows) or 'ifconfig' (Linux/Mac) to find your IP." << std::endl;
        }

        // Handle internet access if enabled
        if (config.allowInternetAccess)
        {
            std::cout << "\nInternet access is ENABLED - attempting to configure internet connectivity..." << std::endl;

            // Try to configure UPnP port forwarding
            bool upnpSuccess = configureUPnPPortForwarding(config.port);

            // Get public IP address
            std::cout << "\nDetecting public IP address..." << std::endl;
            std::string publicIP = getPublicIPAddress();

            if (!publicIP.empty())
            {
                std::cout << "\nInternet accessible addresses:" << std::endl;
                if (upnpSuccess)
                {
                    std::cout << "   • http://" << publicIP << ":" << config.port << " (internet access via UPnP)" << std::endl;
                }
                else
                {
                    std::cout << "   • http://" << publicIP << ":" << config.port << " (internet access - manual port forwarding required)" << std::endl;
                    std::cout << "     Note: You need to manually configure port forwarding in your router for port " << config.port << std::endl;
                }

                std::cout << "\nIMPORTANT SECURITY NOTICE:" << std::endl;
                std::cout << "   Your server is accessible from the INTERNET! Ensure:" << std::endl;
                std::cout << "   - Strong authentication is enabled" << std::endl;
                std::cout << "   - Rate limiting is configured" << std::endl;
                std::cout << "   - Only necessary endpoints are exposed" << std::endl;
                std::cout << "   - Monitor access logs regularly" << std::endl;
            }
            else
            {
                std::cout << "   Could not detect public IP address" << std::endl;
                std::cout << "   Internet access may still work if you manually configure port forwarding" << std::endl;
            }
        }
    }
    else
    {
        std::cout << "\nPublic access is DISABLED - server only accessible from this machine" << std::endl;
        std::cout << "   Use --public flag or set allow_public_access: true in config to enable external access" << std::endl;
        std::cout << "   Use --internet flag or set allow_internet_access: true in config to enable internet access" << std::endl;
    }

    ServerLogger::logInfo("\nCore endpoints:");
    ServerLogger::logInfo("  GET  /v1/health              - Health status");
    ServerLogger::logInfo("  GET  /models                 - List available models");
    ServerLogger::logInfo("  POST /v1/chat/completions    - Chat completions (OpenAI compatible)");
    ServerLogger::logInfo("  POST /v1/completions         - Text completions (OpenAI compatible)");
    ServerLogger::logInfo("  POST /v1/embeddings          - Text embeddings (OpenAI compatible)");
    ServerLogger::logInfo("  GET  /engines                - List engines");
    ServerLogger::logInfo("  POST /engines                - Add new engine");
    ServerLogger::logInfo("  GET  /engines/{id}/status    - Engine status");
    ServerLogger::logInfo("  DELETE /engines/{id}         - Remove engine");

    ServerLogger::logInfo("\nDownload endpoints:");
    ServerLogger::logInfo("  GET  /downloads                       - List all downloads status");
    ServerLogger::logInfo("  GET  /downloads/{model_id}            - Get specific download progress");
    ServerLogger::logInfo("  DELETE /downloads/{model_id}          - Cancel specific download");
    ServerLogger::logInfo("  POST /downloads/{model_id}/cancel     - Cancel download");
    ServerLogger::logInfo("  POST /downloads/{model_id}/pause      - Pause download");
    ServerLogger::logInfo("  POST /downloads/{model_id}/resume     - Resume download");

    ServerLogger::logInfo("\nDocument & RAG endpoints:");
    ServerLogger::logInfo("  POST /retrieve                       - Document retrieval endpoint");
    ServerLogger::logInfo("  GET  /retrieve/test                  - Document retrieval diagnostic test");
    ServerLogger::logInfo("  POST /api/v1/documents               - Add documents to collection");
    ServerLogger::logInfo("  DELETE /api/v1/documents             - Remove documents from collection");
    ServerLogger::logInfo("  POST /parse-pdf                     - PDF parse endpoint");
    ServerLogger::logInfo("  POST /parse-docx                    - DOCX parse endpoint");
    ServerLogger::logInfo("  POST /chunking                      - Document chunking endpoint");

    ServerLogger::logInfo("\nWorkflow endpoints:");
    ServerLogger::logInfo("  POST /sequential-workflows                  - Create sequential workflow");
    ServerLogger::logInfo("  GET  /sequential-workflows                  - List sequential workflows");
    ServerLogger::logInfo("  GET  /sequential-workflows/{id}             - Get workflow details");
    ServerLogger::logInfo("  POST /sequential-workflows/{id}/execute     - Execute workflow");
    ServerLogger::logInfo("  GET  /sequential-workflows/{id}/result      - Get workflow result");
    ServerLogger::logInfo("  GET  /sequential-workflows/{id}/status      - Get workflow status");
    ServerLogger::logInfo("  POST /sequential-workflows/{id}/cancel      - Cancel workflow");
    ServerLogger::logInfo("  DELETE /sequential-workflows/{id}           - Delete workflow");
    ServerLogger::logInfo("  GET  /sequential-workflows/metrics          - Workflow executor metrics");
    ServerLogger::logInfo("  POST /sequential-workflows/from-template    - Create workflow from template");

    ServerLogger::logInfo("\nOrchestration endpoints:");
    ServerLogger::logInfo("  POST /orchestration/workflows         - Create orchestration workflow");
    ServerLogger::logInfo("  POST /orchestration/execute           - Execute orchestration workflow");
    ServerLogger::logInfo("  GET  /orchestration/status            - Orchestration status");

    ServerLogger::logInfo("\nServer endpoints:");
    ServerLogger::logInfo("  GET  /server-logs                    - Retrieve server logs");
    
    if (config.search.enabled)
    {
        ServerLogger::logInfo("\nSearch endpoints:");
        ServerLogger::logInfo("  POST /search                         - Internet search endpoint");
    }
    
    if (agentManager) {
        ServerLogger::logInfo("\nAgent System endpoints:");
        ServerLogger::logInfo("  GET  /api/v1/agents                      - List all agents");
        ServerLogger::logInfo("  POST /api/v1/agents                      - Create new agent");
        ServerLogger::logInfo("  GET  /api/v1/agents/{id}                 - Get agent details");
        ServerLogger::logInfo("  POST /api/v1/agents/{id}/start           - Start agent");
        ServerLogger::logInfo("  POST /api/v1/agents/{id}/stop            - Stop agent");
        ServerLogger::logInfo("  DELETE /api/v1/agents/{id}               - Delete agent");
        ServerLogger::logInfo("  POST /api/v1/agents/{id}/execute         - Execute agent function");
        ServerLogger::logInfo("  POST /api/v1/agents/{id}/execute-async   - Execute agent function async");
        ServerLogger::logInfo("  GET  /api/v1/agents/jobs/{job_id}/status - Get async job status");
        ServerLogger::logInfo("  GET  /api/v1/agents/jobs/{job_id}/result - Get async job result");
        ServerLogger::logInfo("  POST /api/v1/agents/messages/send        - Send message to agent");
        ServerLogger::logInfo("  POST /api/v1/agents/messages/broadcast   - Broadcast message");
        ServerLogger::logInfo("  POST /v1/agents/{id}/chat/completions    - OpenAI compatible agent chat");
        ServerLogger::logInfo("  GET  /api/v1/agents/system/status        - Agent system status");
        ServerLogger::logInfo("  GET  /api/v1/agents/system/metrics       - Agent system metrics");
        if (agentOrchestrator) {
            ServerLogger::logInfo("\nAgent Orchestration endpoints:");
            ServerLogger::logInfo("  POST /api/v1/orchestration/workflows              - Create workflow");
            ServerLogger::logInfo("  GET  /api/v1/orchestration/workflows              - List workflows");
            ServerLogger::logInfo("  POST /api/v1/orchestration/workflows/{id}/execute - Execute workflow");
            ServerLogger::logInfo("  POST /api/v1/orchestration/workflows/{id}/execute-async - Execute workflow async");
            ServerLogger::logInfo("  GET  /api/v1/orchestration/workflows/{id}/status  - Get workflow status");
            ServerLogger::logInfo("  GET  /api/v1/orchestration/workflows/{id}/result  - Get workflow result");
            ServerLogger::logInfo("  POST /api/v1/orchestration/collaboration-groups   - Create collaboration group");
            ServerLogger::logInfo("  POST /api/v1/orchestration/collaboration-groups/{id}/execute - Execute group");
            ServerLogger::logInfo("  GET  /api/v1/orchestration/status                 - Orchestration status");
            ServerLogger::logInfo("  GET  /api/v1/orchestration/metrics                - Orchestration metrics");
            ServerLogger::logInfo("  POST /api/v1/agents/system/reload                 - Reload agent system");
        }
    }
    if (config.auth.enableAuth)
    {
        ServerLogger::logInfo("\nAuthentication endpoints:");
        ServerLogger::logInfo("  GET  /v1/auth/config                 - Get authentication configuration");
        ServerLogger::logInfo("  PUT  /v1/auth/config                 - Update authentication configuration");
        ServerLogger::logInfo("  GET  /v1/auth/stats                  - Get authentication statistics");
        ServerLogger::logInfo("  POST /v1/auth/clear                  - Clear rate limit data");
    }
    if (config.enableMetrics)
    {
        ServerLogger::logInfo("\nMetrics endpoints:");
        ServerLogger::logInfo("  GET  /metrics                        - Combined system and completion metrics");
        ServerLogger::logInfo("  GET  /v1/metrics                     - Combined system and completion metrics");
        ServerLogger::logInfo("  GET  /metrics/system                 - System monitoring metrics only");
        ServerLogger::logInfo("  GET  /v1/metrics/system              - System monitoring metrics only");
        ServerLogger::logInfo("  GET  /metrics/completion             - Completion performance metrics only");
        ServerLogger::logInfo("  GET  /v1/metrics/completion          - Completion performance metrics only");
        ServerLogger::logInfo("  GET  /metrics/completion/{engine_id} - Engine-specific completion metrics");
    }
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
    std::cout.flush();
    
    // Log that we're entering the main loop
    ServerLogger::logInfo("Entering main server loop");

    // Main server loop
    while (keep_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check if server thread is still running
        try {
            server.checkServerThread();
        } catch (const std::exception& e) {
            std::cerr << "Server thread error detected: " << e.what() << std::endl;
            ServerLogger::logError("Server thread error detected: %s", e.what());
            keep_running = false;
            break;
        }
    }

    std::cout << "Shutting down server..." << std::endl;
    ServerLogger::logInfo("Received shutdown signal, stopping server...");
    
    // Shutdown agent system first
    if (agentOrchestrator) {
        ServerLogger::logInfo("Stopping agent orchestrator...");
        agentOrchestrator->stop();
    }
    if (agentManager) {
        ServerLogger::logInfo("Stopping agent manager...");
        agentManager->stop();
    }
    
    server.shutdown();
    std::cout << "Server stopped." << std::endl;

    return 0;
}

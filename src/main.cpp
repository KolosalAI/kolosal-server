#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <vector>
#include <filesystem>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wininet.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
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

                        char ipStr[INET6_ADDRSTRLEN];
                        DWORD ipStrLen = INET6_ADDRSTRLEN;

                        if (WSAAddressToStringA(unicast->Address.lpSockaddr,
                                                unicast->Address.iSockaddrLength,
                                                nullptr, ipStr, &ipStrLen) == 0)
                        {
                            std::string ip(ipStr);
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
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    keep_running = false;
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
    // Load configuration from command line arguments
    ServerConfig config;
    if (!config.loadFromArgs(argc, argv))
    {
        return config.validate() ? 0 : 1; // Return 0 for help/version, 1 for errors
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
    // Initialize the server
    ServerAPI &server = ServerAPI::instance();

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

    if (!server.init(config.port, bindHost))
    {
        std::cerr << "Failed to initialize server on " << bindHost << ":" << config.port << std::endl;
        return 1;
    } // Configure authentication if enabled
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
            auth::AuthMiddleware::ApiKeyConfig apiKeyConfig;
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
    } // Load models if specified
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
                                                              modelConfig.loadAtStartup);

            if (success)
            {
                // Check if this was a URL that started an async download
                if (is_valid_url(modelConfig.path) && !std::filesystem::exists(generate_download_path(modelConfig.path, "./models")))
                {
                    std::cout << "✓ Model '" << modelConfig.id << "' download started (async)" << std::endl;
                    ServerLogger::logInfo("Model '%s' download started from URL: %s", modelConfig.id.c_str(), modelConfig.path.c_str());
                    asyncDownloads++;
                }
                else if (modelConfig.loadAtStartup)
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
    std::cout << "\nAvailable endpoints:" << std::endl;
    std::cout << "  GET  /health                 - Health status" << std::endl;
    std::cout << "  GET  /models                 - List available models" << std::endl;
    std::cout << "  POST /v1/chat/completions    - Chat completions (OpenAI compatible)" << std::endl;
    std::cout << "  POST /v1/completions         - Text completions (OpenAI compatible)" << std::endl;
    std::cout << "  GET  /engines                - List engines" << std::endl;
    std::cout << "  POST /engines                - Add new engine" << std::endl;
    std::cout << "  GET  /engines/{id}/status    - Engine status" << std::endl;
    std::cout << "  DELETE /engines/{id}         - Remove engine" << std::endl;

    if (config.auth.enableAuth)
    {
        std::cout << "\nAuthentication endpoints:" << std::endl;
        std::cout << "  GET  /v1/auth/config         - Get authentication configuration" << std::endl;
        std::cout << "  PUT  /v1/auth/config         - Update authentication configuration" << std::endl;
        std::cout << "  GET  /v1/auth/stats          - Get authentication statistics" << std::endl;
        std::cout << "  POST /v1/auth/clear          - Clear rate limit data" << std::endl;
    }
    if (config.enableMetrics)
    {
        std::cout << "\nMetrics endpoints:" << std::endl;
        std::cout << "  GET  /metrics                - System monitoring metrics" << std::endl;
        std::cout << "  GET  /v1/metrics             - System monitoring metrics" << std::endl;
        std::cout << "  GET  /system/metrics         - System monitoring metrics" << std::endl;
        std::cout << "  GET  /completion-metrics     - Completion performance metrics" << std::endl;
        std::cout << "  GET  /v1/completion-metrics  - Completion performance metrics" << std::endl;
    }

    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;

    // Main server loop
    while (keep_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down server..." << std::endl;
    server.shutdown();
    std::cout << "Server stopped." << std::endl;

    return 0;
}

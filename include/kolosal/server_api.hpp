#pragma once

#include <string>
#include <memory>
#include <chrono>

#include "export.hpp"
#include "server_config.hpp"

namespace kolosal {
    // Forward declarations
    class NodeManager;
    class AutoSetupManager;
    struct ServerConfig;
    namespace auth {
        class AuthMiddleware;
    }
    namespace agents {
        class YAMLConfigurableAgentManager;
        class AgentOrchestrator;
    }
    namespace retrieval {
        class DocumentService;
    }

    class KOLOSAL_SERVER_API ServerAPI
    {
    public:
        // Singleton pattern
        static ServerAPI &instance();

        // Delete copy/move constructors and assignments
        ServerAPI(const ServerAPI &) = delete;
        ServerAPI &operator=(const ServerAPI &) = delete;
        ServerAPI(ServerAPI &&) = delete;
        ServerAPI &operator=(ServerAPI &&) = delete;
        
        // Initialize and start server
        bool init(const std::string &port, const std::string &host = "0.0.0.0", std::chrono::seconds idleTimeout = std::chrono::seconds(300));
        bool init(const std::string &port, const std::string &host, std::chrono::seconds idleTimeout, const ServerConfig& config);
        void shutdown();
        
        // Feature management
        void enableMetrics();
        void enableSearch(const SearchConfig& config);
        
        // NodeManager access
        NodeManager& getNodeManager();
        const NodeManager& getNodeManager() const;
        
        // AuthMiddleware access
        auth::AuthMiddleware& getAuthMiddleware();
        const auth::AuthMiddleware& getAuthMiddleware() const;
        
        // AuthManager access (returns AuthMiddleware for compatibility)
        auth::AuthMiddleware& getAuthManager();
        const auth::AuthMiddleware& getAuthManager() const;

        // Agent system access
        agents::YAMLConfigurableAgentManager& getAgentManager();
        const agents::YAMLConfigurableAgentManager& getAgentManager() const;
        
        agents::AgentOrchestrator& getAgentOrchestrator();
        const agents::AgentOrchestrator& getAgentOrchestrator() const;

        // Auto-setup system access
        AutoSetupManager& getAutoSetupManager();
        const AutoSetupManager& getAutoSetupManager() const;

        // Document retrieval system access
        retrieval::DocumentService& getDocumentService();
        const retrieval::DocumentService& getDocumentService() const;

    private:
        ServerAPI();
        ~ServerAPI();

        class KOLOSAL_SERVER_API Impl;
        // Suppress C4251 warning for pImpl unique_ptr
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)
#endif
        std::unique_ptr<Impl> pImpl;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    };

} // namespace kolosal
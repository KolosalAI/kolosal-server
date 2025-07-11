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
    namespace auth {
        class AuthMiddleware;
    }
    namespace agents {
        class YAMLConfigurableAgentManager;
        class AgentOrchestrator;
    }

    class KOLOSAL_SERVER_API ServerAPI
    {
    public:
        // Singleton pattern
        static ServerAPI &instance();

        // Delete copy/move constructors and assignments
<<<<<<< HEAD
        ServerAPI(const ServerAPI&) = delete;
        ServerAPI& operator=(const ServerAPI&) = delete;
        ServerAPI(ServerAPI&&) = delete;
        ServerAPI& operator=(ServerAPI&&) = delete;
        
        // Initialize and start server
        bool init(const std::string& port, const std::string& host = "0.0.0.0");
=======
        ServerAPI(const ServerAPI &) = delete;
        ServerAPI &operator=(const ServerAPI &) = delete;
        ServerAPI(ServerAPI &&) = delete;
        ServerAPI &operator=(ServerAPI &&) = delete;        // Initialize and start server
        bool init(const std::string &port, const std::string &host = "0.0.0.0", std::chrono::seconds idleTimeout = std::chrono::seconds(300));
>>>>>>> e45dbad8fd9aafe89b192b548e51b6598f36470d
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

    private:
        ServerAPI();
        ~ServerAPI();

<<<<<<< HEAD
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
=======
        class Impl;
#pragma warning(push)
#pragma warning(disable: 4251)
        std::unique_ptr<Impl> pImpl;
#pragma warning(pop)
>>>>>>> e45dbad8fd9aafe89b192b548e51b6598f36470d
    };

} // namespace kolosal
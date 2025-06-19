#pragma once

#include <string>
#include <memory>

#include "export.hpp"

namespace kolosal
{

    // Forward declarations
    class NodeManager;
    namespace auth
    {
        class AuthMiddleware;
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
        ServerAPI &operator=(ServerAPI &&) = delete;        // Initialize and start server
        bool init(const std::string &port, const std::string &host = "0.0.0.0");
        void shutdown();
        
        // Feature management
        void enableMetrics();

        // NodeManager access
        NodeManager &getNodeManager();
        const NodeManager &getNodeManager() const;

        // AuthMiddleware access
        auth::AuthMiddleware &getAuthMiddleware();
        const auth::AuthMiddleware &getAuthMiddleware() const;

    private:
        ServerAPI();
        ~ServerAPI();

        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace kolosal
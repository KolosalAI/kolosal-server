#pragma once

#include <string>
#include <memory>

#include "export.hpp"

namespace kolosal {    // Forward declarations
    class NodeManager;
    namespace auth {
        class AuthMiddleware;
        class AuthManager;
    }

    class KOLOSAL_SERVER_API ServerAPI {
    public:
        // Singleton pattern
        static ServerAPI& instance();

        // Delete copy/move constructors and assignments
        ServerAPI(const ServerAPI&) = delete;
        ServerAPI& operator=(const ServerAPI&) = delete;
        ServerAPI(ServerAPI&&) = delete;
        ServerAPI& operator=(ServerAPI&&) = delete;        // Initialize and start server
        bool init(const std::string& port);
        void shutdown();        // NodeManager access
        NodeManager& getNodeManager();
        const NodeManager& getNodeManager() const;        // AuthMiddleware access
        auth::AuthMiddleware& getAuthMiddleware();
        const auth::AuthMiddleware& getAuthMiddleware() const;

        // AuthManager access
        auth::AuthManager& getAuthManager();
        const auth::AuthManager& getAuthManager() const;

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
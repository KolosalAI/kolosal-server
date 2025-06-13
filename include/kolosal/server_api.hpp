#pragma once

#include <string>
#include <memory>

#include "export.hpp"

namespace kolosal {

    // Forward declaration
    class NodeManager;

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
        void shutdown();

        // NodeManager access
        NodeManager& getNodeManager();
        const NodeManager& getNodeManager() const;

    private:
        ServerAPI();
        ~ServerAPI();

        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace kolosal
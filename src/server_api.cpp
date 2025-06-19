#include "kolosal/server_api.hpp"
#include "kolosal/server.hpp"
#include "kolosal/routes/chat_completion_route.hpp"
#include "kolosal/routes/completion_route.hpp"
#include "kolosal/routes/add_engine_route.hpp"
#include "kolosal/routes/list_engines_route.hpp"
#include "kolosal/routes/remove_engine_route.hpp"
#include "kolosal/routes/engine_status_route.hpp"
#include "kolosal/routes/health_status_route.hpp"
#include "kolosal/routes/auth_config_route.hpp"
#include "kolosal/routes/system_metrics_route.hpp"
#include "kolosal/routes/completion_metrics_route.hpp"
#include "kolosal/routes/combined_metrics_route.hpp"
#include "kolosal/routes/download_progress_route.hpp"
#include "kolosal/routes/downloads_status_route.hpp"
#include "kolosal/routes/cancel_download_route.hpp"
#include "kolosal/routes/cancel_all_downloads_route.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include <memory>
#include <stdexcept>

namespace kolosal
{

    class ServerAPI::Impl
    {
    public:
        std::unique_ptr<Server> server;
        std::unique_ptr<NodeManager> nodeManager;

        Impl()
            : nodeManager(std::make_unique<NodeManager>())
        {
        }
    };

    ServerAPI::ServerAPI() : pImpl(std::make_unique<Impl>()) {}

    ServerAPI::~ServerAPI()
    {
        shutdown();
    }

    ServerAPI &ServerAPI::instance()
    {
        static ServerAPI instance;
        return instance;
    }
    bool ServerAPI::init(const std::string &port, const std::string &host)
    {
        try
        {
            ServerLogger::logInfo("Initializing server on %s:%s", host.c_str(), port.c_str());

            pImpl->server = std::make_unique<Server>(port, host);
            if (!pImpl->server->init())
            {
                ServerLogger::logError("Failed to initialize server");
                return false;
            } // Register routes
            ServerLogger::logInfo("Registering routes");
            pImpl->server->addRoute(std::make_unique<ChatCompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<CompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<AddEngineRoute>());
            pImpl->server->addRoute(std::make_unique<ListEnginesRoute>());
            pImpl->server->addRoute(std::make_unique<RemoveEngineRoute>());
            pImpl->server->addRoute(std::make_unique<EngineStatusRoute>());
            pImpl->server->addRoute(std::make_unique<HealthStatusRoute>());
            pImpl->server->addRoute(std::make_unique<AuthConfigRoute>());
            pImpl->server->addRoute(std::make_unique<DownloadProgressRoute>());
            pImpl->server->addRoute(std::make_unique<DownloadsStatusRoute>());
            pImpl->server->addRoute(std::make_unique<CancelDownloadRoute>());
            pImpl->server->addRoute(std::make_unique<CancelAllDownloadsRoute>());

            // Register metrics routes
            pImpl->server->addRoute(std::make_unique<CombinedMetricsRoute>()); // Handles /metrics and /v1/metrics
            pImpl->server->addRoute(std::make_unique<SystemMetricsRoute>());   // Handles /system/metrics

            // Start server in a background thread
            std::thread([this]()
                        {
                ServerLogger::logInfo("Starting server main loop");
                pImpl->server->run(); })
                .detach();

            return true;
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Failed to initialize server: %s", ex.what());
            return false;
        }
    }
    void ServerAPI::shutdown()
    {
        if (pImpl->server)
        {
            ServerLogger::logInfo("Shutting down server");

            // Wait for all download threads to complete (this will cancel them first)
            try
            {
                auto &download_manager = DownloadManager::getInstance();
                ServerLogger::logInfo("Stopping all downloads and waiting for threads to finish...");
                download_manager.waitForAllDownloads();
            }
            catch (const std::exception &ex)
            {
                ServerLogger::logError("Error during download shutdown: %s", ex.what());
            }

            // Shutdown the server
            ServerLogger::logInfo("Shutting down HTTP server");
            pImpl->server.reset();
            ServerLogger::logInfo("Server shutdown complete");
        }
    }
    void ServerAPI::enableMetrics()
    {
        if (!pImpl->server)
        {
            throw std::runtime_error("Server not initialized - call init() first");
        }

        ServerLogger::logInfo("Enabling system metrics monitoring");
        pImpl->server->addRoute(std::make_unique<SystemMetricsRoute>());

        ServerLogger::logInfo("Enabling completion metrics monitoring");
        pImpl->server->addRoute(std::make_unique<CompletionMetricsRoute>());
    }

    NodeManager &ServerAPI::getNodeManager()
    {
        return *pImpl->nodeManager;
    }
    const NodeManager &ServerAPI::getNodeManager() const
    {
        return *pImpl->nodeManager;
    }

    auth::AuthMiddleware &ServerAPI::getAuthMiddleware()
    {
        if (!pImpl->server)
        {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
    }

    const auth::AuthMiddleware &ServerAPI::getAuthMiddleware() const
    {
        if (!pImpl->server)
        {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
    }

} // namespace kolosal
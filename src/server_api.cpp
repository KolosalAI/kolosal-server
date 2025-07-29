#include "kolosal/server_api.hpp"
#include "kolosal/server.hpp"
#include "kolosal/routes/oai_completions_route.hpp"
#include "kolosal/routes/completion_route.hpp"
#include "kolosal/routes/embedding_route.hpp"
#include "kolosal/routes/models_route.hpp"
#include "kolosal/routes/engines_route.hpp"
#include "kolosal/routes/health_status_route.hpp"
#include "kolosal/routes/auth_config_route.hpp"
#include "kolosal/routes/server_logs_route.hpp"
#include "kolosal/routes/agents_route.hpp"

#include "kolosal/routes/parse_document_route.hpp"
#include "kolosal/routes/documents_route.hpp"
#include "kolosal/routes/retrieve_route.hpp"
#include "kolosal/routes/internet_search_route.hpp"
#include "kolosal/routes/downloads_route.hpp"
#include "kolosal/routes/chunking_route.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/auto_setup_manager.hpp"
#include "kolosal/agents/agent_orchestrator.hpp"
#include <memory>
#include <stdexcept>
#include <thread>

namespace kolosal
{

    class ServerAPI::Impl
    {
    public:
        std::unique_ptr<Server> server;
        std::shared_ptr<NodeManager> nodeManager;
        std::shared_ptr<agents::YAMLConfigurableAgentManager> agentManager;
        std::shared_ptr<agents::AgentOrchestrator> agentOrchestrator;
        std::unique_ptr<retrieval::DocumentService> documentService;
        std::unique_ptr<AutoSetupManager> autoSetupManager;

        Impl()
        {
        }
        
        void initNodeManager(std::chrono::seconds idleTimeout)
        {
            nodeManager = std::make_shared<NodeManager>(idleTimeout);
        }

        void initAgentManager()
        {
            agentManager = std::make_shared<agents::YAMLConfigurableAgentManager>();
        }

        void initAgentOrchestrator()
        {
            agentOrchestrator = std::make_shared<agents::AgentOrchestrator>(agentManager);
        }

        void initDocumentService(const DatabaseConfig& dbConfig)
        {
            documentService = std::make_unique<retrieval::DocumentService>(dbConfig);
        }

        void initAutoSetupManager()
        {
            autoSetupManager = std::make_unique<AutoSetupManager>(nodeManager, agentManager);
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
    bool ServerAPI::init(const std::string &port, const std::string &host, std::chrono::seconds idleTimeout)
    {
        try
        {
            ServerLogger::logInfo("Initializing server on %s:%s with idle timeout: %lld seconds", host.c_str(), port.c_str(), idleTimeout.count());

            // Initialize NodeManager with configured idle timeout
            pImpl->initNodeManager(idleTimeout);
            
            // Initialize AgentManager
            pImpl->initAgentManager();

            // Initialize AgentOrchestrator
            pImpl->initAgentOrchestrator();

            // Initialize DocumentService with default database config
            DatabaseConfig defaultDbConfig;
            pImpl->initDocumentService(defaultDbConfig);

            // Initialize AutoSetupManager
            pImpl->initAutoSetupManager();

            pImpl->server = std::make_unique<Server>(port, host);
            if (!pImpl->server->init())
            {
                ServerLogger::logError("Failed to initialize server");
                return false;
            }
            
            // Register routes
            ServerLogger::logInfo("Registering routes");
            pImpl->server->addRoute(std::make_unique<routes::AgentsRoute>(pImpl->agentManager));
            pImpl->server->addRoute(std::make_unique<OaiCompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<CompletionRoute>());
            pImpl->server->addRoute(std::make_unique<EmbeddingRoute>());
            pImpl->server->addRoute(std::make_unique<ModelsRoute>());
            pImpl->server->addRoute(std::make_unique<EnginesRoute>());
            pImpl->server->addRoute(std::make_unique<HealthStatusRoute>());
            pImpl->server->addRoute(std::make_unique<AuthConfigRoute>());
            pImpl->server->addRoute(std::make_unique<ServerLogsRoute>());
            pImpl->server->addRoute(std::make_unique<DownloadsRoute>());
            pImpl->server->addRoute(std::make_unique<ParseDocumentRoute>());
            pImpl->server->addRoute(std::make_unique<DocumentsRoute>());
            pImpl->server->addRoute(std::make_unique<RetrieveRoute>());
            pImpl->server->addRoute(std::make_unique<ChunkingRoute>());

            ServerLogger::logInfo("Routes registered successfully");

            // Start server in a background thread
            std::thread([this]() {
                ServerLogger::logInfo("Starting server main loop");
                pImpl->server->run(); 
            }).detach();

            return true;
        }
        catch (const std::exception &ex)
        {
            ServerLogger::logError("Failed to initialize server: %s", ex.what());
            return false;
        }
    }
    
    bool ServerAPI::init(const std::string &port, const std::string &host, std::chrono::seconds idleTimeout, const ServerConfig& config)
    {
        try
        {
            ServerLogger::logInfo("Initializing server on %s:%s with idle timeout: %lld seconds", host.c_str(), port.c_str(), idleTimeout.count());

            // Initialize NodeManager with configured idle timeout
            pImpl->initNodeManager(idleTimeout);
            
            // Initialize AgentManager
            pImpl->initAgentManager();

            // Initialize AgentOrchestrator
            pImpl->initAgentOrchestrator();

            // Initialize DocumentService with database config
            pImpl->initDocumentService(config.database);

            // Initialize AutoSetupManager
            pImpl->initAutoSetupManager();

            pImpl->server = std::make_unique<Server>(port, host);
            if (!pImpl->server->init())
            {
                ServerLogger::logError("Failed to initialize server");
                return false;
            }
            
            // Register routes
            ServerLogger::logInfo("Registering routes");
            pImpl->server->addRoute(std::make_unique<routes::AgentsRoute>(pImpl->agentManager));
            pImpl->server->addRoute(std::make_unique<OaiCompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<CompletionRoute>());
            pImpl->server->addRoute(std::make_unique<EmbeddingRoute>());
            pImpl->server->addRoute(std::make_unique<ModelsRoute>());
            pImpl->server->addRoute(std::make_unique<EnginesRoute>());
            pImpl->server->addRoute(std::make_unique<HealthStatusRoute>());
            pImpl->server->addRoute(std::make_unique<AuthConfigRoute>());
            pImpl->server->addRoute(std::make_unique<ServerLogsRoute>());
            pImpl->server->addRoute(std::make_unique<DownloadsRoute>());
            pImpl->server->addRoute(std::make_unique<ParseDocumentRoute>());
            pImpl->server->addRoute(std::make_unique<DocumentsRoute>());
            pImpl->server->addRoute(std::make_unique<RetrieveRoute>());
            pImpl->server->addRoute(std::make_unique<ChunkingRoute>());

            ServerLogger::logInfo("Routes registered successfully");

            // Start server in a background thread
            std::thread([this]() {
                ServerLogger::logInfo("Starting server main loop");
                pImpl->server->run(); 
            }).detach();

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

        ServerLogger::logInfo("Metrics functionality not yet implemented");
        // TODO: Add metrics routes when they are implemented
        // pImpl->server->addRoute(std::make_unique<SystemMetricsRoute>());
        // pImpl->server->addRoute(std::make_unique<CompletionMetricsRoute>());
    }

    void ServerAPI::enableSearch(const SearchConfig &config)
    {
        if (!pImpl->server)
        {
            throw std::runtime_error("Server not initialized - call init() first");
        }

        ServerLogger::logInfo("Enabling internet search endpoint");
        pImpl->server->addRoute(std::make_unique<InternetSearchRoute>(config));
    }

    bool ServerAPI::isServerThreadRunning() const
    {
        if (!pImpl->server)
        {
            return false;
        }
        
        // Check if the server is still running
        // The Server class would need to provide a method to check this
        // For now, we'll return true if server exists
        return true;
    }

    void ServerAPI::checkServerThread()
    {
        if (!isServerThreadRunning())
        {
            throw std::runtime_error("Server thread has died or is not running");
        }
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

    auth::AuthMiddleware &ServerAPI::getAuthManager()
    {
        return getAuthMiddleware();
    }

    const auth::AuthMiddleware &ServerAPI::getAuthManager() const
    {
        return getAuthMiddleware();
    }

    agents::YAMLConfigurableAgentManager &ServerAPI::getAgentManager()
    {
        if (!pImpl->agentManager)
        {
            throw std::runtime_error("Agent manager not initialized");
        }
        return *pImpl->agentManager;
    }

    const agents::YAMLConfigurableAgentManager &ServerAPI::getAgentManager() const
    {
        if (!pImpl->agentManager)
        {
            throw std::runtime_error("Agent manager not initialized");
        }
        return *pImpl->agentManager;
    }

    agents::AgentOrchestrator &ServerAPI::getAgentOrchestrator()
    {
        if (!pImpl->agentOrchestrator)
        {
            throw std::runtime_error("Agent orchestrator not initialized");
        }
        return *pImpl->agentOrchestrator;
    }

    const agents::AgentOrchestrator &ServerAPI::getAgentOrchestrator() const
    {
        if (!pImpl->agentOrchestrator)
        {
            throw std::runtime_error("Agent orchestrator not initialized");
        }
        return *pImpl->agentOrchestrator;
    }

    void ServerAPI::setAgentManager(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    {
        pImpl->agentManager = manager;
    }

    void ServerAPI::setAgentOrchestrator(std::shared_ptr<agents::AgentOrchestrator> orchestrator)
    {
        pImpl->agentOrchestrator = orchestrator;
    }

    void ServerAPI::finalizeRoutes()
    {
        if (!pImpl->server)
        {
            throw std::runtime_error("Server not initialized");
        }
        // Routes are already finalized in init()
    }

    retrieval::DocumentService &ServerAPI::getDocumentService()
    {
        if (!pImpl->documentService)
        {
            throw std::runtime_error("Document service not initialized");
        }
        return *pImpl->documentService;
    }

    const retrieval::DocumentService &ServerAPI::getDocumentService() const
    {
        if (!pImpl->documentService)
        {
            throw std::runtime_error("Document service not initialized");
        }
        return *pImpl->documentService;
    }

    AutoSetupManager &ServerAPI::getAutoSetupManager()
    {
        if (!pImpl->autoSetupManager)
        {
            throw std::runtime_error("Auto setup manager not initialized");
        }
        return *pImpl->autoSetupManager;
    }

    const AutoSetupManager &ServerAPI::getAutoSetupManager() const
    {
        if (!pImpl->autoSetupManager)
        {
            throw std::runtime_error("Auto setup manager not initialized");
        }
        return *pImpl->autoSetupManager;
    }

} // namespace kolosal
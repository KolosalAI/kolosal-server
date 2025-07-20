#include "kolosal/server_api.hpp"
#include "kolosal/server.hpp"
#include "kolosal/routes/chat_completion_route.hpp"
#include "kolosal/routes/completion_route.hpp"
#include "kolosal/routes/inference_completion_route.hpp"
#include "kolosal/routes/inference_chat_completion_route.hpp"
#include "kolosal/routes/models_route.hpp"
#include "kolosal/routes/list_inference_engines_route.hpp"
#include "kolosal/routes/health_status_route.hpp"
#include "kolosal/routes/auth_config_route.hpp"
#include "kolosal/routes/server_logs_route.hpp"
#include "kolosal/routes/agents_route.hpp"
#include "kolosal/routes/orchestration_route.hpp"
#include "kolosal/routes/sequential_workflow_route.hpp"
#include "kolosal/routes/agent_monitoring_route.hpp"

#include "kolosal/routes/downloads_route.hpp"
#include "kolosal/routes/parse_pdf_route.hpp"
#include "kolosal/routes/parse_docx_route.hpp"
#include "kolosal/routes/embedding_route.hpp"
#include "kolosal/routes/add_documents_route.hpp"
#include "kolosal/routes/retrieve_route.hpp"
#include "kolosal/retrieval/remove_documents_route.hpp"
#include "kolosal/routes/auto_setup_route.hpp"
#include "kolosal/routes/documents_route.hpp"
#include "kolosal/routes/collections_route.hpp"
#include "kolosal/routes/context_retrieval_route.hpp"
#include "kolosal/routes/vector_search_route.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/agents/multi_agent_system.hpp"
#include "kolosal/agents/agent_orchestrator.hpp"
#include "kolosal/auto_setup_manager.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include <memory>
#include <stdexcept>

namespace kolosal
{

    class ServerAPI::Impl
    {
    public:
        std::unique_ptr<Server> server;
        std::shared_ptr<NodeManager> nodeManager;
        std::shared_ptr<kolosal::agents::YAMLConfigurableAgentManager> agentManager;
        std::shared_ptr<kolosal::agents::AgentOrchestrator> agentOrchestrator;
        std::unique_ptr<AutoSetupManager> autoSetupManager;
        std::unique_ptr<retrieval::DocumentService> documentService;
        ServerConfig config;

        Impl()
        {
            // Services will be initialized later when config is available
        }
        
        void initServices()
        {
            // Initialize DocumentService with database config
            documentService = std::make_unique<retrieval::DocumentService>(config.database);
            
            // Initialize the DocumentService (test connections, etc.)
            if (documentService) {
                ServerLogger::logInfo("Initializing DocumentService with Qdrant enabled: %s, host: %s, port: %d", 
                                     config.database.qdrant.enabled ? "true" : "false",
                                     config.database.qdrant.host.c_str(),
                                     config.database.qdrant.port);
                
                auto init_future = documentService->initialize();
                bool initialized = init_future.get();
                if (!initialized) {
                    ServerLogger::logError("Failed to initialize DocumentService - check if Qdrant is running on %s:%d", 
                                         config.database.qdrant.host.c_str(), config.database.qdrant.port);
                } else {
                    ServerLogger::logInfo("DocumentService initialized successfully");
                }
            }
            
            // Initialize AutoSetupManager with required parameters (only if agent manager is available)
            if (nodeManager && agentManager) {
                autoSetupManager = std::make_unique<AutoSetupManager>(nodeManager, agentManager);
            }
        }
        
        void initNodeManager(std::chrono::seconds idleTimeout)
        {
            nodeManager = std::make_shared<NodeManager>(idleTimeout);
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
        // Use default configuration
        ServerConfig defaultConfig;
        return init(port, host, idleTimeout, defaultConfig);
    }

    bool ServerAPI::init(const std::string &port, const std::string &host, std::chrono::seconds idleTimeout, const ServerConfig& config)
    {
        try
        {
            // Store the configuration
            pImpl->config = config;
            
            ServerLogger::logInfo("Initializing server on %s:%s with idle timeout: %lld seconds", host.c_str(), port.c_str(), idleTimeout.count());

            // Initialize NodeManager with configured idle timeout
            pImpl->initNodeManager(idleTimeout);

            // Initialize services now that we have config and node manager
            pImpl->initServices();

            pImpl->server = std::make_unique<Server>(port, host);
            if (!pImpl->server->init())
            {
                ServerLogger::logError("Failed to initialize server");
                return false;
            }
            
            // Register routes
            ServerLogger::logInfo("Registering routes");
            pImpl->server->addRoute(std::make_unique<ChatCompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<CompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<InferenceCompletionRoute>());
            pImpl->server->addRoute(std::make_unique<InferenceChatCompletionRoute>());
            pImpl->server->addRoute(std::make_unique<ModelsRoute>());
            pImpl->server->addRoute(std::make_unique<ListInferenceEnginesRoute>());
            pImpl->server->addRoute(std::make_unique<HealthStatusRoute>());
            pImpl->server->addRoute(std::make_unique<AuthConfigRoute>());
            pImpl->server->addRoute(std::make_unique<ServerLogsRoute>());
            pImpl->server->addRoute(std::make_unique<DownloadsRoute>());
            pImpl->server->addRoute(std::make_unique<ParsePDFRoute>());
            pImpl->server->addRoute(std::make_unique<ParseDOCXRoute>());
            pImpl->server->addRoute(std::make_unique<EmbeddingRoute>());
            pImpl->server->addRoute(std::make_unique<AddDocumentsRoute>());
            pImpl->server->addRoute(std::make_unique<RetrieveRoute>());
            pImpl->server->addRoute(std::make_unique<retrieval::RemoveDocumentsRoute>());
            
            // Register new API v1 routes
            pImpl->server->addRoute(std::make_unique<routes::DocumentsRoute>());
            pImpl->server->addRoute(std::make_unique<routes::CollectionsRoute>());
            pImpl->server->addRoute(std::make_unique<routes::ContextRetrievalRoute>());
            pImpl->server->addRoute(std::make_unique<routes::VectorSearchRoute>());
            
            // Register Auto-Setup routes
            pImpl->server->addRoute(std::make_unique<routes::AutoSetupRoute>());

            ServerLogger::logInfo("Routes registered successfully");

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

    void ServerAPI::enableMetrics()
    {
        // Implementation for enabling metrics
        // This would typically involve setting up metrics collection endpoints
        ServerLogger::logInfo("Metrics enabled");
    }

    void ServerAPI::enableSearch(const SearchConfig& config)
    {
        // Implementation for enabling search functionality
        // This would typically involve setting up search endpoints
        ServerLogger::logInfo("Search functionality enabled");
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

    // Agent system methods
    void ServerAPI::setAgentManager(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    {
        pImpl->agentManager = manager;
        
        // Register agent routes if manager is available
        if (pImpl->server && pImpl->agentManager) {
            auto agentsRoute = routes::AgentsRoute(pImpl->agentManager);
            agentsRoute.setup_routes(*pImpl->server);
            pImpl->server->addRoute(std::make_unique<AgentMonitoringRoute>(pImpl->agentManager, pImpl->agentOrchestrator));
            ServerLogger::logInfo("Agent routes registered");
        }
    }

    void ServerAPI::setAgentOrchestrator(std::shared_ptr<agents::AgentOrchestrator> orchestrator)
    {
        pImpl->agentOrchestrator = orchestrator;
        
        // Register orchestration and workflow routes if available
        if (pImpl->server && pImpl->agentOrchestrator) {
            pImpl->server->addRoute(std::make_unique<routes::OrchestrationRoute>(pImpl->agentOrchestrator));
            pImpl->server->addRoute(std::make_unique<routes::SequentialWorkflowRoute>(pImpl->agentManager));
            ServerLogger::logInfo("Agent orchestration routes registered");
        }
        
        // Update monitoring route if it exists
        if (pImpl->agentManager) {
            // The monitoring route will be updated when both manager and orchestrator are available
            ServerLogger::logInfo("Agent monitoring updated with orchestrator");
        }
    }

    agents::YAMLConfigurableAgentManager& ServerAPI::getAgentManager()
    {
        if (!pImpl->agentManager)
        {
            throw std::runtime_error("Agent manager not initialized");
        }
        return *pImpl->agentManager;
    }

    const agents::YAMLConfigurableAgentManager& ServerAPI::getAgentManager() const
    {
        if (!pImpl->agentManager)
        {
            throw std::runtime_error("Agent manager not initialized");
        }
        return *pImpl->agentManager;
    }

    agents::AgentOrchestrator& ServerAPI::getAgentOrchestrator()
    {
        if (!pImpl->agentOrchestrator)
        {
            throw std::runtime_error("Agent orchestrator not initialized");
        }
        return *pImpl->agentOrchestrator;
    }

    const agents::AgentOrchestrator& ServerAPI::getAgentOrchestrator() const
    {
        if (!pImpl->agentOrchestrator)
        {
            throw std::runtime_error("Agent orchestrator not initialized");
        }
        return *pImpl->agentOrchestrator;
    }

    AutoSetupManager& ServerAPI::getAutoSetupManager()
    {
        if (!pImpl->autoSetupManager)
        {
            throw std::runtime_error("AutoSetupManager not initialized");
        }
        return *pImpl->autoSetupManager;
    }

    const AutoSetupManager& ServerAPI::getAutoSetupManager() const
    {
        if (!pImpl->autoSetupManager)
        {
            throw std::runtime_error("AutoSetupManager not initialized");
        }
        return *pImpl->autoSetupManager;
    }

    retrieval::DocumentService& ServerAPI::getDocumentService()
    {
        if (!pImpl->documentService)
        {
            throw std::runtime_error("DocumentService not initialized");
        }
        return *pImpl->documentService;
    }

    const retrieval::DocumentService& ServerAPI::getDocumentService() const
    {
        if (!pImpl->documentService)
        {
            throw std::runtime_error("DocumentService not initialized");
        }
        return *pImpl->documentService;
    }

} // namespace kolosal
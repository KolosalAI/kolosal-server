#include "kolosal/server_api.hpp"
#include "kolosal/server.hpp"
#include "kolosal/routes/chat_completion_route.hpp"
#include "kolosal/routes/completion_route.hpp"
#include "kolosal/routes/embedding_route.hpp"
#include "kolosal/routes/models_route.hpp"
#include "kolosal/routes/inference_completion_route.hpp"
#include "kolosal/routes/inference_chat_completion_route.hpp"
#include "kolosal/routes/add_engine_route.hpp"
#include "kolosal/routes/list_engines_route.hpp"
#include "kolosal/routes/remove_engine_route.hpp"
#include "kolosal/routes/engine_status_route.hpp"
#include "kolosal/routes/health_status_route.hpp"
#include "kolosal/routes/auth_config_route.hpp"
<<<<<<< HEAD
#include "kolosal/routes/agents_route.hpp"
#include "kolosal/routes/orchestration_route.hpp"
#include "kolosal/routes/sequential_workflow_route.hpp"
#include "kolosal/routes/auto_setup_route.hpp"
#include "kolosal/routes/system_metrics_route.hpp"
#include "kolosal/routes/completion_metrics_route.hpp"
#include "kolosal/routes/combined_metrics_route.hpp"
=======

>>>>>>> e45dbad8fd9aafe89b192b548e51b6598f36470d
#include "kolosal/routes/download_progress_route.hpp"
#include "kolosal/routes/downloads_status_route.hpp"
#include "kolosal/routes/cancel_download_route.hpp"
#include "kolosal/routes/pause_download_route.hpp"
#include "kolosal/routes/resume_download_route.hpp"
#include "kolosal/routes/cancel_all_downloads_route.hpp"
#include "kolosal/routes/parse_pdf_route.hpp"
#include "kolosal/routes/parse_docx_route.hpp"
#include "kolosal/routes/add_documents_route.hpp"
#include "kolosal/retrieval/remove_documents_route.hpp"
#include "kolosal/routes/retrieve_route.hpp"
#include "kolosal/routes/internet_search_route.hpp"
#include "kolosal/download_manager.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/agents/agent_interfaces.hpp"
#include "kolosal/agents/agent_data.hpp"
#include "kolosal/agents/multi_agent_system.hpp"
#include "kolosal/agents/agent_orchestrator.hpp"
#include "kolosal/auto_setup_manager.hpp"
#include <memory>
#include <stdexcept>
#include <thread>

namespace kolosal
{
    class ServerAPI::Impl
    {
    public:
        std::unique_ptr<Server> server;
        std::unique_ptr<NodeManager> nodeManager;
        std::shared_ptr<agents::YAMLConfigurableAgentManager> agentManager;
        std::shared_ptr<agents::AgentOrchestrator> agentOrchestrator;
        std::unique_ptr<AutoSetupManager> autoSetupManager;

        Impl()
<<<<<<< HEAD
            : nodeManager(std::make_unique<NodeManager>()),
              agentManager(std::make_shared<agents::YAMLConfigurableAgentManager>()),
              agentOrchestrator(std::make_shared<agents::AgentOrchestrator>(agentManager))
=======
>>>>>>> e45dbad8fd9aafe89b192b548e51b6598f36470d
        {
            // Initialize auto-setup manager after node manager and agent manager
            autoSetupManager = std::make_unique<AutoSetupManager>(
                std::shared_ptr<NodeManager>(nodeManager.get(), [](NodeManager*) {}), // Non-owning shared_ptr
                agentManager
            );
        }
        
        void initNodeManager(std::chrono::seconds idleTimeout)
        {
            nodeManager = std::make_unique<NodeManager>(idleTimeout);
        }
    };

    ServerAPI::ServerAPI() : pImpl(std::make_unique<Impl>()) {}

    ServerAPI::~ServerAPI()
    {
        this->shutdown();
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
            pImpl->server->addRoute(std::make_unique<EmbeddingRoute>());
            pImpl->server->addRoute(std::make_unique<ModelsRoute>());
            pImpl->server->addRoute(std::make_unique<InferenceCompletionRoute>());
            pImpl->server->addRoute(std::make_unique<InferenceChatCompletionRoute>());
            pImpl->server->addRoute(std::make_unique<AddEngineRoute>());
            pImpl->server->addRoute(std::make_unique<ListEnginesRoute>());
            pImpl->server->addRoute(std::make_unique<RemoveEngineRoute>());
            pImpl->server->addRoute(std::make_unique<EngineStatusRoute>());
            pImpl->server->addRoute(std::make_unique<HealthStatusRoute>());
            pImpl->server->addRoute(std::make_unique<AuthConfigRoute>());
            
            // Register agent system routes
            ServerLogger::logInfo("Registering agent system routes");
            auto agentsRoute = std::make_unique<routes::AgentsRoute>(pImpl->agentManager);
            agentsRoute->setup_routes(*pImpl->server);
            
            // Add orchestration route directly (it implements IRoute interface)
            auto orchestrationRoute = std::make_unique<routes::OrchestrationRoute>(pImpl->agentOrchestrator);
            pImpl->server->addRoute(std::move(orchestrationRoute));
            
            // Add sequential workflow route
            ServerLogger::logInfo("Registering sequential workflow route");
            auto sequentialWorkflowRoute = std::make_unique<routes::SequentialWorkflowRoute>(pImpl->agentManager);
            pImpl->server->addRoute(std::move(sequentialWorkflowRoute));

            // Add auto-setup route for user convenience
            ServerLogger::logInfo("Registering auto-setup route");
            auto autoSetupRoute = std::make_unique<routes::AutoSetupRoute>();
            pImpl->server->addRoute(std::move(autoSetupRoute));
            
            // Register retrieval and monitoring routes
            pImpl->server->addRoute(std::make_unique<DownloadProgressRoute>());
            pImpl->server->addRoute(std::make_unique<DownloadsStatusRoute>());
            pImpl->server->addRoute(std::make_unique<CancelDownloadRoute>());
            pImpl->server->addRoute(std::make_unique<PauseDownloadRoute>());
            pImpl->server->addRoute(std::make_unique<ResumeDownloadRoute>());
            pImpl->server->addRoute(std::make_unique<CancelAllDownloadsRoute>());
            pImpl->server->addRoute(std::make_unique<ParsePDFRoute>());
            pImpl->server->addRoute(std::make_unique<ParseDOCXRoute>());            
            pImpl->server->addRoute(std::make_unique<AddDocumentsRoute>());
            pImpl->server->addRoute(std::make_unique<kolosal::retrieval::RemoveDocumentsRoute>());
            pImpl->server->addRoute(std::make_unique<RetrieveRoute>());

            ServerLogger::logInfo("Routes registered successfully");

            // Start agent systems
            ServerLogger::logInfo("Starting agent systems");
            
            // Load agent configuration and start agent manager
            std::string config_path = "config/agents.yaml";
            
            // Try alternative paths if the default doesn't exist
            std::vector<std::string> config_paths = {
                "config/agents.yaml",
                "agents.yaml",
                "../config/agents.yaml",
                "./config/agents.yaml"
            };
            
            bool config_loaded = false;
            for (const auto& path : config_paths) {
                if (pImpl->agentManager->load_configuration(path)) {
                    ServerLogger::logInfo("Agent configuration loaded successfully from %s", path.c_str());
                    pImpl->agentManager->start();
                    config_loaded = true;
                    break;
                }
            }
            
            if (!config_loaded) {
                ServerLogger::logWarning("Failed to load agent configuration from any of the attempted paths");
                ServerLogger::logInfo("Creating default agent configuration...");
                // Start agent manager anyway - agents can be created via API
                pImpl->agentManager->start();
            }
            
            pImpl->agentOrchestrator->start();

            // Perform automatic setup after all systems are initialized
            ServerLogger::logInfo("Performing automatic server setup...");
            if (pImpl->autoSetupManager->perform_auto_setup()) {
                ServerLogger::logInfo("✅ Automatic setup completed successfully!");
            } else {
                ServerLogger::logWarning("⚠️  Automatic setup completed with some issues");
            }

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
        if (pImpl->agentOrchestrator)
        {
            ServerLogger::logInfo("Shutting down agent orchestrator");
            pImpl->agentOrchestrator->stop();
        }
        
        if (pImpl->agentManager)
        {
            ServerLogger::logInfo("Shutting down agent manager");
            pImpl->agentManager->stop();
        }
        
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
        if (!pImpl->server)
        {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
    }

    const auth::AuthMiddleware &ServerAPI::getAuthManager() const
    {
        if (!pImpl->server)
        {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
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

    AutoSetupManager &ServerAPI::getAutoSetupManager()
    {
        if (!pImpl->autoSetupManager)
        {
            throw std::runtime_error("Auto-setup manager not initialized");
        }
        return *pImpl->autoSetupManager;
    }

    const AutoSetupManager &ServerAPI::getAutoSetupManager() const
    {
        if (!pImpl->autoSetupManager)
        {
            throw std::runtime_error("Auto-setup manager not initialized");
        }
        return *pImpl->autoSetupManager;
    }

} // namespace kolosal
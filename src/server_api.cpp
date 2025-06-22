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
#include "kolosal/routes/agents_route.hpp"
#include "kolosal/routes/orchestration_route.hpp"
#include "kolosal/node_manager.h"
#include "kolosal/logger.hpp"
#include "kolosal/agents/multi_agent_system.hpp"
#include "kolosal/agents/agent_orchestrator.hpp"
#include <memory>
#include <stdexcept>

namespace kolosal
{    class ServerAPI::Impl
    {
    public:
        std::unique_ptr<Server> server;
        std::unique_ptr<NodeManager> nodeManager;
        std::shared_ptr<agents::YAMLConfigurableAgentManager> agentManager;
        std::shared_ptr<agents::AgentOrchestrator> agentOrchestrator;

        Impl()
            : nodeManager(std::make_unique<NodeManager>()),
              agentManager(std::make_shared<agents::YAMLConfigurableAgentManager>()),
              agentOrchestrator(std::make_shared<agents::AgentOrchestrator>(agentManager))
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

    bool ServerAPI::init(const std::string &port)
    {
        try
        {
            ServerLogger::logInfo("Initializing server on port %s", port.c_str());

            pImpl->server = std::make_unique<Server>(port);
            if (!pImpl->server->init())
            {
                ServerLogger::logError("Failed to initialize server");
                return false;
            }            // Register routes
            ServerLogger::logInfo("Registering routes");            pImpl->server->addRoute(std::make_unique<ChatCompletionsRoute>());
            pImpl->server->addRoute(std::make_unique<CompletionsRoute>());
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
            pImpl->server->addRoute(std::move(orchestrationRoute));            // Start agent systems
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
    }    void ServerAPI::shutdown()
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
            // Implement server shutdown mechanism
            pImpl->server.reset();
        }
    }

    NodeManager &ServerAPI::getNodeManager()
    {
        return *pImpl->nodeManager;
    }    const NodeManager &ServerAPI::getNodeManager() const
    {
        return *pImpl->nodeManager;
    }

    auth::AuthMiddleware& ServerAPI::getAuthMiddleware()
    {
        if (!pImpl->server) {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
    }

    const auth::AuthMiddleware& ServerAPI::getAuthMiddleware() const
    {
        if (!pImpl->server) {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
    }    auth::AuthMiddleware& ServerAPI::getAuthManager()
    {
        if (!pImpl->server) {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
    }

    const auth::AuthMiddleware& ServerAPI::getAuthManager() const
    {
        if (!pImpl->server) {
            throw std::runtime_error("Server not initialized");
        }
        return pImpl->server->getAuthMiddleware();
    }

    agents::YAMLConfigurableAgentManager& ServerAPI::getAgentManager()
    {
        if (!pImpl->agentManager) {
            throw std::runtime_error("Agent manager not initialized");
        }
        return *pImpl->agentManager;
    }

    const agents::YAMLConfigurableAgentManager& ServerAPI::getAgentManager() const
    {
        if (!pImpl->agentManager) {
            throw std::runtime_error("Agent manager not initialized");
        }
        return *pImpl->agentManager;
    }

    agents::AgentOrchestrator& ServerAPI::getAgentOrchestrator()
    {
        if (!pImpl->agentOrchestrator) {
            throw std::runtime_error("Agent orchestrator not initialized");
        }
        return *pImpl->agentOrchestrator;
    }

    const agents::AgentOrchestrator& ServerAPI::getAgentOrchestrator() const
    {
        if (!pImpl->agentOrchestrator) {
            throw std::runtime_error("Agent orchestrator not initialized");
        }
        return *pImpl->agentOrchestrator;
    }

} // namespace kolosal
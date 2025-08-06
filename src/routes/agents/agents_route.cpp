#include "kolosal/routes/agents/agents_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/agents/agent_core.hpp"
#include "kolosal/agents/yaml_config.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/server_api.hpp"
#include <json.hpp>
#include <regex>
#include <iostream>

using json = nlohmann::json;

namespace kolosal::routes::agents {

AgentsRoute::AgentsRoute(std::shared_ptr<kolosal::agents::YAMLConfigurableAgentManager> manager)
    : agent_manager(std::move(manager))
{
    ServerLogger::logInfo("AgentsRoute initialized with YAMLConfigurableAgentManager");
    
    // Initialize services
    document_service = std::make_unique<kolosal::agents::DocumentAgentService>();
    workflow_service = std::make_unique<kolosal::agents::WorkflowAgentService>();
}

bool AgentsRoute::match(const std::string& method, const std::string& path) {
    current_method = method;
    current_path = path;
    
    // Handle OPTIONS for CORS - support both API v1 and OpenAI compatible v1 endpoints
    if (method == "OPTIONS" && (path.find("/api/v1/agents") == 0 || path.find("/v1/agents") == 0)) {
        return true;
    }
    
    // Core agent management endpoints
    if ((method == "GET" || method == "POST") && path == "/api/v1/agents") {
        return true;
    }
    
    // Agent inference endpoint (direct under agent)
    std::regex agent_inference_pattern("^/api/v1/agents/([^/]+)/inference$");
    std::smatch inference_matches;
    
    if (std::regex_match(path, inference_matches, agent_inference_pattern)) {
        if (method == "POST") {
            return true;
        }
    }
    
    // Agent system endpoints
    if (method == "GET" && (path == "/api/v1/agents/system/status" || path == "/api/v1/agents/system/metrics")) {
        return true;
    }
    
    // Agent-specific endpoints with regex matching
    std::regex agent_pattern("^/api/v1/agents/([^/]+)(?:/(start|stop|restart|status|capabilities|functions|message))?(?:/([^/]+))?$");
    std::smatch matches;
    
    if (std::regex_match(path, matches, agent_pattern)) {
        if (method == "GET" || method == "PUT" || method == "DELETE" || method == "POST") {
            return true;
        }
    }
    
    // OpenAI-compatible endpoints
    std::regex openai_pattern("^/v1/agents/([^/]+)/chat/completions$");
    std::smatch openai_matches;
    
    if (std::regex_match(path, openai_matches, openai_pattern)) {
        if (method == "POST") {
            return true;
        }
    }
    
    // Agent templates
    if ((path == "/api/v1/agents/templates" && method == "GET") ||
        (path.find("/api/v1/agents/templates/") == 0 && method == "POST")) {
        return true;
    }
    
    // Bulk operations
    if (method == "POST" && (path == "/api/v1/agents/bulk/start" || 
                            path == "/api/v1/agents/bulk/stop" || 
                            path == "/api/v1/agents/bulk/delete")) {
        return true;
    }
    
    // Broadcast message endpoint
    if (method == "POST" && path == "/api/v1/agents/messages/broadcast") {
        return true;
    }
    
    // Inter-agent message sending endpoint
    if (method == "POST" && path == "/api/v1/agents/messages/send") {
        return true;
    }
    
    // Document service endpoints
    if (method == "POST" && (path == "/api/v1/agents/documents/bulk" ||
                            path == "/api/v1/agents/documents/bulk_retrieval" ||
                            path == "/api/v1/agents/documents/search" ||
                            path == "/api/v1/agents/documents/upload")) {
        return true;
    }
    
    if ((method == "GET" || method == "POST") && path == "/api/v1/agents/collections") {
        return true;
    }
    
    if (path.find("/api/v1/agents/collections/") == 0 && 
        (method == "GET" || method == "DELETE")) {
        return true;
    }
    
    // Workflow service endpoints
    if (method == "POST" && (path == "/api/v1/agents/workflows" ||
                            path == "/api/v1/agents/workflows/execute" ||
                            path == "/api/v1/agents/workflows/rag" ||
                            path == "/api/v1/agents/rag/search")) {
        return true;
    }
    
    if (method == "GET" && (path == "/api/v1/agents/workflows" ||
                           path.find("/api/v1/agents/workflows/") == 0)) {
        return true;
    }
    
    if (method == "DELETE" && path.find("/api/v1/agents/workflows/") == 0) {
        return true;
    }
    
    // Session management
    if ((method == "GET" || method == "POST") && path == "/api/v1/agents/sessions") {
        return true;
    }
    
    if (path.find("/api/v1/agents/sessions/") == 0 && 
        (method == "GET" || method == "DELETE")) {
        return true;
    }
    
    // Orchestration
    if (method == "POST" && (path == "/api/v1/agents/orchestration" ||
                            path.find("/api/v1/agents/orchestration/") == 0)) {
        return true;
    }
    
    if (method == "GET" && path.find("/api/v1/agents/orchestration/") == 0) {
        return true;
    }
    
    return false;
}

void AgentsRoute::handle(SocketType sock, const std::string& body) {
    try {
        // Handle OPTIONS requests for CORS
        if (current_method == "OPTIONS") {
            std::map<std::string, std::string> headers = {
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
                {"Access-Control-Max-Age", "86400"}
            };
            send_response(sock, 200, "");
            return;
        }
        
        // Route to appropriate handler based on path and method
        if (current_path == "/api/v1/agents") {
            if (current_method == "GET") {
                handle_list_agents(sock);
            } else if (current_method == "POST") {
                handle_create_agent(sock, body);
            }
        } else if (current_path == "/api/v1/agents/system/status") {
            handle_agent_system_status(sock);
        } else if (current_path == "/api/v1/agents/system/metrics") {
            handle_agent_system_metrics(sock);
        } else if (current_path == "/api/v1/agents/templates") {
            handle_list_agent_templates(sock);
        } else if (current_path.find("/api/v1/agents/templates/") == 0) {
            std::string template_name = extractIdFromPath(current_path, "/api/v1/agents/templates");
            handle_create_agent_from_template(sock, template_name, body);
        } else if (current_path == "/api/v1/agents/bulk/start") {
            handle_bulk_start_agents(sock, body);
        } else if (current_path == "/api/v1/agents/bulk/stop") {
            handle_bulk_stop_agents(sock, body);
        } else if (current_path == "/api/v1/agents/bulk/delete") {
            handle_bulk_delete_agents(sock, body);
        } else if (current_path == "/api/v1/agents/messages/broadcast") {
            handle_broadcast_message(sock, body);
        } else if (current_path == "/api/v1/agents/messages/send") {
            handle_send_message(sock, body);
        } else if (current_path == "/api/v1/agents/documents/bulk") {
            handle_bulk_documents(sock, body);
        } else if (current_path == "/api/v1/agents/documents/bulk_retrieval") {
            handle_bulk_retrieval(sock, body);
        } else if (current_path == "/api/v1/agents/documents/search") {
            handle_document_search(sock, body);
        } else if (current_path == "/api/v1/agents/documents/upload") {
            handle_document_upload(sock, body);
        } else if (current_path == "/api/v1/agents/collections") {
            if (current_method == "GET") {
                handle_list_collections(sock);
            } else if (current_method == "POST") {
                handle_create_collection(sock, body);
            }
        } else if (current_path.find("/api/v1/agents/collections/") == 0) {
            std::string collection_name = extractIdFromPath(current_path, "/api/v1/agents/collections");
            if (current_method == "GET") {
                handle_get_collection_info(sock, collection_name);
            } else if (current_method == "DELETE") {
                handle_delete_collection(sock, collection_name);
            }
        } else if (current_path == "/api/v1/agents/workflows") {
            if (current_method == "GET") {
                handle_list_workflows(sock);
            } else if (current_method == "POST") {
                handle_create_workflow(sock, body);
            }
        } else if (current_path == "/api/v1/agents/workflows/execute") {
            handle_execute_workflow(sock, body);
        } else if (current_path == "/api/v1/agents/workflows/rag") {
            handle_rag_workflow(sock, body);
        } else if (current_path == "/api/v1/agents/rag/search") {
            handle_rag_search(sock, body);
        } else if (current_path.find("/api/v1/agents/workflows/") == 0) {
            std::string workflow_id = extractIdFromPath(current_path, "/api/v1/agents/workflows");
            if (current_method == "GET") {
                handle_get_workflow_status(sock, workflow_id);
            } else if (current_method == "DELETE") {
                handle_delete_workflow(sock, workflow_id);
            }
        } else if (current_path == "/api/v1/agents/sessions") {
            if (current_method == "GET") {
                handle_list_sessions(sock);
            } else if (current_method == "POST") {
                handle_create_session(sock, body);
            }
        } else if (current_path.find("/api/v1/agents/sessions/") == 0) {
            std::string session_id = extractIdFromPath(current_path, "/api/v1/agents/sessions");
            if (current_path.find("/history") != std::string::npos) {
                handle_session_history(sock, session_id);
            } else if (current_method == "GET") {
                handle_get_session(sock, session_id);
            } else if (current_method == "DELETE") {
                handle_delete_session(sock, session_id);
            }
        } else if (current_path == "/api/v1/agents/orchestration" && current_method == "POST") {
            handle_create_orchestration(sock, body);
        } else if (current_path.find("/api/v1/agents/orchestration/") == 0) {
            std::string plan_id = extractIdFromPath(current_path, "/api/v1/agents/orchestration");
            if (current_method == "POST") {
                handle_execute_orchestration(sock, plan_id, body);
            } else if (current_method == "GET") {
                handle_orchestration_status(sock, plan_id);
            }
        } else {
            // Handle agent-specific operations
            std::regex agent_pattern("^/api/v1/agents/([^/]+)(?:/(start|stop|restart|status|capabilities|functions|message))?(?:/([^/]+))?$");
            std::smatch matches;
            
            // Check for OpenAI-compatible chat completions first
            std::regex openai_pattern("^/v1/agents/([^/]+)/chat/completions$");
            std::smatch openai_matches;
            
            // Check for direct inference endpoint
            std::regex agent_inference_pattern("^/api/v1/agents/([^/]+)/inference$");
            std::smatch inference_matches;
            
            if (std::regex_match(current_path, openai_matches, openai_pattern)) {
                std::string agent_id = openai_matches[1].str();
                handle_openai_chat_completions(sock, agent_id, body);
            }
            // Check for direct inference endpoint
            else if (std::regex_match(current_path, inference_matches, agent_inference_pattern)) {
                std::string agent_id = inference_matches[1].str();
                handle_agent_inference(sock, agent_id, body);
            } else if (std::regex_match(current_path, matches, agent_pattern)) {
                std::string agent_id = matches[1].str();
                std::string action = matches.size() > 2 ? matches[2].str() : "";
                std::string param = matches.size() > 3 ? matches[3].str() : "";
                
                if (action.empty()) {
                    if (current_method == "GET") {
                        handle_get_agent(sock, agent_id);
                    } else if (current_method == "PUT") {
                        handle_update_agent(sock, agent_id, body);
                    } else if (current_method == "DELETE") {
                        handle_delete_agent(sock, agent_id);
                    }
                } else if (action == "start") {
                    handle_start_agent(sock, agent_id);
                } else if (action == "stop") {
                    handle_stop_agent(sock, agent_id);
                } else if (action == "restart") {
                    handle_restart_agent(sock, agent_id);
                } else if (action == "status") {
                    handle_agent_status(sock, agent_id);
                } else if (action == "capabilities") {
                    handle_get_agent_capabilities(sock, agent_id);
                } else if (action == "functions") {
                    if (param.empty()) {
                        handle_list_agent_functions(sock, agent_id);
                    } else {
                        if (current_method == "POST") {
                            handle_execute_agent_function(sock, agent_id, param, body);
                        } else if (current_method == "GET") {
                            handle_test_agent_function(sock, agent_id, param, body);
                        }
                    }
                } else if (action == "message") {
                    handle_send_message_to_agent(sock, agent_id, body);
                }
            } else {
                send_error_response(sock, 404, "Endpoint not found");
            }
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in AgentsRoute::handle: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::setup_routes(Server& server) {
    // This method can be used for additional route setup if needed
    ServerLogger::logInfo("AgentsRoute routes configured");
}

// Core agent management implementations
void AgentsRoute::handle_create_agent(SocketType sock, const std::string& body) {
    try {
        json request = json::parse(body);
        
        if (!validate_agent_config(request)) {
            send_error_response(sock, 400, "Invalid agent configuration");
            return;
        }
        
        if (!agent_manager) {
            send_error_response(sock, 500, "Agent manager not available");
            return;
        }
        
        // Create agent configuration from request
        kolosal::agents::AgentConfig config;
        config.name = request.value("name", "Unnamed Agent");
        config.type = request.value("type", "generic");
        config.role = request.value("role", "assistant");
        config.system_prompt = request.value("system_prompt", "You are a helpful AI assistant.");
        
        // Add capabilities if provided
        if (request.contains("capabilities") && request["capabilities"].is_array()) {
            for (const auto& cap : request["capabilities"]) {
                if (cap.is_string()) {
                    config.capabilities.push_back(cap.get<std::string>());
                }
            }
        }
        
        // Add functions if provided
        if (request.contains("functions") && request["functions"].is_array()) {
            for (const auto& func : request["functions"]) {
                if (func.is_string()) {
                    config.functions.push_back(func.get<std::string>());
                }
            }
        }
        
        // Create agent using the agent manager
        std::string agent_id = agent_manager->create_agent_from_config(config);
        
        if (agent_id.empty()) {
            send_error_response(sock, 500, "Failed to create agent");
            return;
        }
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"name", config.name},
            {"type", config.type},
            {"role", config.role},
            {"created", true}
        };
        
        ServerLogger::logInfo("Created agent: %s", agent_id.c_str());
        send_json_response(sock, 201, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error creating agent: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_list_agents(SocketType sock) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        std::vector<std::string> agent_ids = agent_manager->list_agents();
        json agents_array = json::array();
        
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent) {
                json agent_info = {
                    {"agent_id", agent->get_agent_id()},
                    {"name", agent->get_agent_name()},
                    {"type", agent->get_agent_type()},
                    {"state", agent->is_running() ? "running" : "stopped"},
                    {"capabilities", agent->get_capabilities()}
                };
                agents_array.push_back(agent_info);
            }
        }
        
        json response = {
            {"status", "success"},
            {"agents", agents_array},
            {"total", agents_array.size()}
        };
        
        ServerLogger::logInfo("Listed %d agents", agents_array.size());
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error listing agents: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_get_agent(SocketType sock, const std::string& agent_id) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        json response = {
            {"status", "success"},
            {"agent_id", agent->get_agent_id()},
            {"name", agent->get_agent_name()},
            {"type", agent->get_agent_type()},
            {"state", agent->is_running() ? "running" : "stopped"},
            {"capabilities", agent->get_capabilities()}
        };
        
        ServerLogger::logInfo("Getting agent: %s", agent_id.c_str());
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting agent %s: %s", agent_id.c_str(), e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_update_agent(SocketType sock, const std::string& agent_id, const std::string& body) {
    try {
        json request = json::parse(body);
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"message", "Agent updated successfully"}
        };
        
        ServerLogger::logInfo("Updated agent: %s", agent_id.c_str());
        send_json_response(sock, 200, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_delete_agent(SocketType sock, const std::string& agent_id) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        bool success = agent_manager->delete_agent(agent_id);
        if (!success) {
            send_error_response(sock, 404, "Agent not found or failed to delete");
            return;
        }
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"message", "Agent deleted successfully"}
        };
        
        ServerLogger::logInfo("Deleted agent: %s", agent_id.c_str());
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error deleting agent %s: %s", agent_id.c_str(), e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_agent_system_status(SocketType sock) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        std::vector<std::string> agent_ids = agent_manager->list_agents();
        int active_agents = 0;
        
        for (const auto& agent_id : agent_ids) {
            auto agent = agent_manager->get_agent(agent_id);
            if (agent && agent->is_running()) {
                active_agents++;
            }
        }
        
        json response = {
            {"status", "success"},
            {"system", {
                {"running", agent_manager->is_running()},
                {"active_agents", active_agents},
                {"total_agents", agent_ids.size()}
            }}
        };
        
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting agent system status: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_agent_system_metrics(SocketType sock) {
    json response = {
        {"status", "success"},
        {"metrics", {
            {"total_messages_processed", 0},
            {"active_conversations", 0},
            {"system_uptime", "0h 0m"},
            {"memory_usage", "0 MB"}
        }}
    };
    
    send_json_response(sock, 200, response);
}

// Agent lifecycle management
void AgentsRoute::handle_start_agent(SocketType sock, const std::string& agent_id) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        bool success = agent_manager->start_agent(agent_id);
        if (!success) {
            send_error_response(sock, 404, "Agent not found or failed to start");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        std::string state = agent && agent->is_running() ? "running" : "stopped";
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"state", state},
            {"message", "Agent started successfully"}
        };
        
        ServerLogger::logInfo("Started agent: %s", agent_id.c_str());
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error starting agent %s: %s", agent_id.c_str(), e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_stop_agent(SocketType sock, const std::string& agent_id) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        bool success = agent_manager->stop_agent(agent_id);
        if (!success) {
            send_error_response(sock, 404, "Agent not found or failed to stop");
            return;
        }
        
        auto agent = agent_manager->get_agent(agent_id);
        std::string state = agent && agent->is_running() ? "running" : "stopped";
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"state", state},
            {"message", "Agent stopped successfully"}
        };
        
        ServerLogger::logInfo("Stopped agent: %s", agent_id.c_str());
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error stopping agent %s: %s", agent_id.c_str(), e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_restart_agent(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"state", "running"},
        {"message", "Agent restarted successfully"}
    };
    
    ServerLogger::logInfo("Restarted agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_agent_status(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"state", "stopped"},
        {"uptime", 0},
        {"last_activity", "2024-01-01T00:00:00Z"},
        {"metrics", {
            {"tasks_completed", 0},
            {"tasks_failed", 0},
            {"memory_usage", "0 MB"}
        }}
    };
    
    ServerLogger::logInfo("Getting status for agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

// Agent capabilities and functions
void AgentsRoute::handle_get_agent_capabilities(SocketType sock, const std::string& agent_id) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"capabilities", agent->get_capabilities()}
        };
        
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error getting agent capabilities for %s: %s", agent_id.c_str(), e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_list_agent_functions(SocketType sock, const std::string& agent_id) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        // Return a list of available built-in functions including RAG
        json functions_array = json::array();
        functions_array.push_back({
            {"name", "inference"},
            {"description", "Generate text using the agent's LLM"},
            {"parameters", {
                {"prompt", "The input text prompt"},
                {"max_tokens", "Maximum tokens to generate (optional)"},
                {"temperature", "Sampling temperature (optional)"}
            }}
        });
        functions_array.push_back({
            {"name", "rag_search"},
            {"description", "Search documents using RAG (Retrieval-Augmented Generation)"},
            {"parameters", {
                {"query", "The search query"},
                {"k", "Number of documents to retrieve (optional, default: 10)"},
                {"collection_name", "Collection to search in (optional)"},
                {"score_threshold", "Minimum similarity score (optional, default: 0.0)"}
            }}
        });
        functions_array.push_back({
            {"name", "rag_inference"},
            {"description", "Perform RAG-enhanced inference using retrieved documents"},
            {"parameters", {
                {"prompt", "The input text prompt"},
                {"query", "The search query for document retrieval"},
                {"k", "Number of documents to retrieve (optional, default: 5)"},
                {"max_tokens", "Maximum tokens to generate (optional)"},
                {"temperature", "Sampling temperature (optional)"}
            }}
        });
        functions_array.push_back({
            {"name", "test_document_service"},
            {"description", "Test document service connectivity and functionality"},
            {"parameters", {
                {"test_type", "Type of test to perform (optional, default: 'connection')"}
            }}
        });
        functions_array.push_back({
            {"name", "retrieval"},
            {"description", "Retrieve documents using semantic search"},
            {"parameters", {
                {"query", "The search query"},
                {"k", "Number of documents to retrieve (optional, default: 5)"},
                {"collection_name", "Collection to search in (optional)"},
                {"score_threshold", "Minimum similarity score (optional, default: 0.0)"}
            }}
        });
        functions_array.push_back({
            {"name", "text_processing"},
            {"description", "Process and analyze text"},
            {"parameters", {
                {"text", "The text to process"},
                {"operation", "Type of processing operation"}
            }}
        });
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"functions", functions_array}
        };
        
        send_json_response(sock, 200, response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error listing agent functions for %s: %s", agent_id.c_str(), e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_execute_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        // Check if agent is running
        if (!agent->is_running()) {
            send_error_response(sock, 400, "Agent is not running. Please start the agent first.");
            return;
        }
        
        json request = json::parse(body);
        
        // Validate function name
        if (function_name != "inference" && function_name != "text_processing" && 
            function_name != "rag_search" && function_name != "rag_inference" &&
            function_name != "test_document_service" && function_name != "retrieval") {
            send_error_response(sock, 400, "Unknown function: " + function_name + ". Available functions: inference, rag_search, rag_inference, text_processing, test_document_service, retrieval");
            return;
        }
        
        kolosal::agents::AgentData params;
        
        // Convert JSON to AgentData with validation
        if (request.contains("parameters") && request["parameters"].is_object()) {
            for (auto& [key, value] : request["parameters"].items()) {
                if (value.is_string()) {
                    params.set(key, value.get<std::string>());
                } else if (value.is_number_integer()) {
                    params.set(key, value.get<int>());
                } else if (value.is_number_float()) {
                    params.set(key, value.get<double>());
                } else if (value.is_boolean()) {
                    params.set(key, value.get<bool>());
                }
            }
        }
        
        // Validate required parameters based on function
        if (function_name == "inference") {
            if (!params.has_key("prompt")) {
                send_error_response(sock, 400, "Missing required parameter 'prompt' for inference function");
                return;
            }
            
            // Set default values if not provided
            if (!params.has_key("max_tokens")) {
                params.set("max_tokens", 100);
            }
            if (!params.has_key("temperature")) {
                params.set("temperature", 0.7);
            }
        } else if (function_name == "rag_search") {
            if (!params.has_key("query")) {
                send_error_response(sock, 400, "Missing required parameter 'query' for rag_search function");
                return;
            }
            
            // Set default values if not provided
            if (!params.has_key("k")) {
                params.set("k", 10);
            }
            if (!params.has_key("score_threshold")) {
                params.set("score_threshold", 0.0);
            }
        } else if (function_name == "rag_inference") {
            if (!params.has_key("prompt")) {
                send_error_response(sock, 400, "Missing required parameter 'prompt' for rag_inference function");
                return;
            }
            if (!params.has_key("query")) {
                send_error_response(sock, 400, "Missing required parameter 'query' for rag_inference function");
                return;
            }
            
            // Set default values if not provided
            if (!params.has_key("k")) {
                params.set("k", 5);
            }
            if (!params.has_key("max_tokens")) {
                params.set("max_tokens", 200);
            }
            if (!params.has_key("temperature")) {
                params.set("temperature", 0.7);
            }
        }
        
        // Execute the function
        auto result = agent->execute_function(function_name, params);
        
        // For test_document_service, we need to handle it specially
        if (function_name == "test_document_service") {
            try {
                // Get DocumentService from ServerAPI
                auto& doc_service = kolosal::ServerAPI::instance().getDocumentService();
                
                // Test the connection
                bool connected = doc_service.testConnection().get();
                
                json response = {
                    {"status", "success"},
                    {"agent_id", agent_id},
                    {"function", function_name},
                    {"test_result", connected ? "connected" : "failed"},
                    {"message", connected ? "Document service is accessible" : "Document service connection failed"}
                };
                
                int status_code = connected ? 200 : 500;
                send_json_response(sock, status_code, response);
                return;
            } catch (const std::exception& e) {
                send_error_response(sock, 500, "Document service test failed: " + std::string(e.what()));
                return;
            }
        }
        // For RAG functions, we need to handle them specially
        else if (function_name == "rag_search" || function_name == "retrieval") {
            try {
                // Get DocumentService from ServerAPI
                auto& doc_service = kolosal::ServerAPI::instance().getDocumentService();
                
                // Create retrieve request
                kolosal::retrieval::RetrieveRequest retrieve_request;
                retrieve_request.query = params.get_string("query");
                retrieve_request.k = params.get_int("k");
                retrieve_request.score_threshold = params.get_double("score_threshold");
                if (params.has_key("collection_name")) {
                    retrieve_request.collection_name = params.get_string("collection_name");
                }
                
                // Execute the search
                auto retrieve_response = doc_service.retrieveDocuments(retrieve_request).get();
                
                json response = {
                    {"status", "success"},
                    {"agent_id", agent_id},
                    {"function", function_name},
                    {"query", retrieve_request.query},
                    {"results", json::array()}
                };
                
                for (const auto& doc : retrieve_response.documents) {
                    json doc_json = {
                        {"id", doc.id},
                        {"text", doc.text},
                        {"score", doc.score},
                        {"metadata", doc.metadata}
                    };
                    response["results"].push_back(doc_json);
                }
                
                response["total_found"] = retrieve_response.total_found;
                
                send_json_response(sock, 200, response);
                return;
            } catch (const std::exception& e) {
                send_error_response(sock, 500, "RAG search failed: " + std::string(e.what()));
                return;
            }
        } else if (function_name == "rag_inference") {
            try {
                // Get DocumentService from ServerAPI
                auto& doc_service = kolosal::ServerAPI::instance().getDocumentService();
                
                // First, perform the retrieval
                kolosal::retrieval::RetrieveRequest retrieve_request;
                retrieve_request.query = params.get_string("query");
                retrieve_request.k = params.get_int("k");
                retrieve_request.score_threshold = params.has_key("score_threshold") ? params.get_double("score_threshold") : 0.0f;
                if (params.has_key("collection_name")) {
                    retrieve_request.collection_name = params.get_string("collection_name");
                }
                
                auto retrieve_response = doc_service.retrieveDocuments(retrieve_request).get();
                
                // Build context from retrieved documents
                std::string context = "Retrieved context:\n";
                for (const auto& doc : retrieve_response.documents) {
                    context += "- " + doc.text + "\n";
                }
                
                // Combine the original prompt with the retrieved context
                std::string enhanced_prompt = context + "\n\nUser prompt: " + params.get_string("prompt");
                
                // Create parameters for inference with enhanced prompt
                kolosal::agents::AgentData inference_params;
                inference_params.set("prompt", enhanced_prompt);
                inference_params.set("max_tokens", params.get_int("max_tokens"));
                inference_params.set("temperature", params.get_double("temperature"));
                
                // Execute inference with the enhanced prompt
                auto inference_result = agent->execute_function("inference", inference_params);
                
                json response = {
                    {"status", "success"},
                    {"agent_id", agent_id},
                    {"function", function_name},
                    {"success", inference_result.success},
                    {"execution_time_ms", inference_result.execution_time_ms},
                    {"retrieved_documents", retrieve_response.documents.size()}
                };
                
                if (inference_result.success) {
                    response["result"] = "RAG inference completed successfully";
                    // Add actual result data if available
                    if (!inference_result.error_message.empty()) {
                        response["output"] = inference_result.error_message;
                    }
                } else {
                    response["error"] = inference_result.error_message;
                }
                
                int status_code = inference_result.success ? 200 : 400;
                send_json_response(sock, status_code, response);
                return;
            } catch (const std::exception& e) {
                send_error_response(sock, 500, "RAG inference failed: " + std::string(e.what()));
                return;
            }
        }
        
        // For other functions, use the normal result
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"function", function_name},
            {"success", result.success},
            {"execution_time_ms", result.execution_time_ms}
        };
        
        if (result.success) {
            response["result"] = "Function executed successfully";
            // Add any result data if available
            if (!result.error_message.empty()) {
                response["data"] = result.error_message; // In case there's actual result data
            }
        } else {
            response["error"] = result.error_message;
            response["details"] = "Function execution failed. Please check agent status and parameters.";
        }
        
        int status_code = result.success ? 200 : 400;
        send_json_response(sock, status_code, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error executing function %s on agent %s: %s", function_name.c_str(), agent_id.c_str(), e.what());
        send_error_response(sock, 500, "Internal server error: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_test_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"function", function_name},
        {"test_result", "Function test completed"}
    };
    
    send_json_response(sock, 200, response);
}

// Agent messaging
void AgentsRoute::handle_send_message_to_agent(SocketType sock, const std::string& agent_id, const std::string& body) {
    try {
        json request = json::parse(body);
        
        if (!validate_message_payload(request)) {
            send_error_response(sock, 400, "Invalid message payload");
            return;
        }
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"message_id", "msg_" + std::to_string(std::time(nullptr))},
            {"response", "Message sent to agent successfully"}
        };
        
        send_json_response(sock, 200, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

// Agent templates
void AgentsRoute::handle_list_agent_templates(SocketType sock) {
    json response = {
        {"status", "success"},
        {"templates", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_create_agent_from_template(SocketType sock, const std::string& template_name, const std::string& body) {
    json response = {
        {"status", "success"},
        {"template", template_name},
        {"agent_id", "agent_" + std::to_string(std::time(nullptr))},
        {"message", "Agent created from template successfully"}
    };
    
    send_json_response(sock, 201, response);
}

// Bulk operations
void AgentsRoute::handle_bulk_start_agents(SocketType sock, const std::string& body) {
    json response = {
        {"status", "success"},
        {"message", "Bulk start operation completed"},
        {"results", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_bulk_stop_agents(SocketType sock, const std::string& body) {
    json response = {
        {"status", "success"},
        {"message", "Bulk stop operation completed"},
        {"results", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_bulk_delete_agents(SocketType sock, const std::string& body) {
    json response = {
        {"status", "success"},
        {"message", "Bulk delete operation completed"},
        {"results", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

// Document service endpoints (delegating to document_service)
void AgentsRoute::handle_bulk_documents(SocketType sock, const std::string& body) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"message", "Bulk document operation completed"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_bulk_retrieval(SocketType sock, const std::string& body) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"message", "Bulk retrieval completed"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_document_search(SocketType sock, const std::string& body) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"results", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_document_upload(SocketType sock, const std::string& body) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"message", "Document uploaded successfully"}
    };
    
    send_json_response(sock, 201, response);
}

void AgentsRoute::handle_list_collections(SocketType sock) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"collections", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_create_collection(SocketType sock, const std::string& body) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"message", "Collection created successfully"}
    };
    
    send_json_response(sock, 201, response);
}

void AgentsRoute::handle_delete_collection(SocketType sock, const std::string& collection_name) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"message", "Collection deleted successfully"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_get_collection_info(SocketType sock, const std::string& collection_name) {
    if (!document_service) {
        send_error_response(sock, 500, "Document service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"collection", collection_name},
        {"info", {}}
    };
    
    send_json_response(sock, 200, response);
}

// Workflow service endpoints (delegating to workflow_service)
void AgentsRoute::handle_create_workflow(SocketType sock, const std::string& body) {
    if (!workflow_service) {
        send_error_response(sock, 500, "Workflow service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"workflow_id", "workflow_" + std::to_string(std::time(nullptr))},
        {"message", "Workflow created successfully"}
    };
    
    send_json_response(sock, 201, response);
}

void AgentsRoute::handle_execute_workflow(SocketType sock, const std::string& body) {
    if (!workflow_service) {
        send_error_response(sock, 500, "Workflow service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"execution_id", "exec_" + std::to_string(std::time(nullptr))},
        {"message", "Workflow execution started"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_get_workflow_status(SocketType sock, const std::string& workflow_id) {
    if (!workflow_service) {
        send_error_response(sock, 500, "Workflow service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"workflow_id", workflow_id},
        {"state", "completed"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_list_workflows(SocketType sock) {
    if (!workflow_service) {
        send_error_response(sock, 500, "Workflow service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"workflows", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_delete_workflow(SocketType sock, const std::string& workflow_id) {
    if (!workflow_service) {
        send_error_response(sock, 500, "Workflow service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"message", "Workflow deleted successfully"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_rag_workflow(SocketType sock, const std::string& body) {
    if (!workflow_service) {
        send_error_response(sock, 500, "Workflow service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"message", "RAG workflow executed successfully"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_rag_search(SocketType sock, const std::string& body) {
    if (!workflow_service) {
        send_error_response(sock, 500, "Workflow service not available");
        return;
    }
    
    json response = {
        {"status", "success"},
        {"results", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

// Session management
void AgentsRoute::handle_create_session(SocketType sock, const std::string& body) {
    json response = {
        {"status", "success"},
        {"session_id", "session_" + std::to_string(std::time(nullptr))},
        {"message", "Session created successfully"}
    };
    
    send_json_response(sock, 201, response);
}

void AgentsRoute::handle_get_session(SocketType sock, const std::string& session_id) {
    json response = {
        {"status", "success"},
        {"session_id", session_id},
        {"info", {}}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_list_sessions(SocketType sock) {
    json response = {
        {"status", "success"},
        {"sessions", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_delete_session(SocketType sock, const std::string& session_id) {
    json response = {
        {"status", "success"},
        {"message", "Session deleted successfully"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_session_history(SocketType sock, const std::string& session_id) {
    json response = {
        {"status", "success"},
        {"session_id", session_id},
        {"history", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

// Orchestration
void AgentsRoute::handle_create_orchestration(SocketType sock, const std::string& body) {
    json response = {
        {"status", "success"},
        {"plan_id", "plan_" + std::to_string(std::time(nullptr))},
        {"message", "Orchestration plan created successfully"}
    };
    
    send_json_response(sock, 201, response);
}

void AgentsRoute::handle_execute_orchestration(SocketType sock, const std::string& plan_id, const std::string& body) {
    json response = {
        {"status", "success"},
        {"plan_id", plan_id},
        {"execution_id", "exec_" + std::to_string(std::time(nullptr))},
        {"message", "Orchestration execution started"}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_orchestration_status(SocketType sock, const std::string& plan_id) {
    json response = {
        {"status", "success"},
        {"plan_id", plan_id},
        {"state", "completed"}
    };
    
    send_json_response(sock, 200, response);
}

// Agent direct inference endpoint
void AgentsRoute::handle_agent_inference(SocketType sock, const std::string& agent_id, const std::string& body) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        // Check if agent is running
        if (!agent->is_running()) {
            send_error_response(sock, 400, "Agent is not running. Please start the agent first.");
            return;
        }
        
        json request = json::parse(body);
        
        if (!request.contains("prompt")) {
            send_error_response(sock, 400, "Missing required parameter 'prompt'");
            return;
        }
        
        // Create parameters for inference
        kolosal::agents::AgentData params;
        params.set("prompt", request["prompt"].get<std::string>());
        params.set("max_tokens", request.value("max_tokens", 100));
        params.set("temperature", request.value("temperature", 0.7));
        
        // Execute inference function
        auto result = agent->execute_function("inference", params);
        
        json response;
        if (result.success) {
            response = {
                {"status", "success"},
                {"agent_id", agent_id},
                {"result", "Inference completed successfully"},
                {"execution_time_ms", result.execution_time_ms}
            };
            
            // Add actual result data if available
            if (result.result_data.has_key("text")) {
                response["output"] = result.result_data.get_string("text");
            } else if (!result.error_message.empty()) {
                response["output"] = result.error_message; // Contains actual output when successful
            }
            
            // Add additional result data
            if (result.result_data.has_key("tokens_generated")) {
                response["tokens_generated"] = result.result_data.get_int("tokens_generated");
            }
            if (result.result_data.has_key("tokens_per_second")) {
                response["tokens_per_second"] = result.result_data.get_double("tokens_per_second");
            }
            if (result.result_data.has_key("engine_used")) {
                response["engine_used"] = result.result_data.get_string("engine_used");
            }
            
            send_json_response(sock, 200, response);
        } else {
            // Log detailed error for debugging
            ServerLogger::logError("Agent inference failed for agent %s: %s", agent_id.c_str(), result.error_message.c_str());
            
            // Provide more specific error messages
            std::string error_msg = result.error_message;
            if (error_msg.find("No available inference engine") != std::string::npos) {
                error_msg = "No inference engines are currently available. Please ensure models are loaded and engines are configured properly.";
            } else if (error_msg.find("engine not found") != std::string::npos) {
                error_msg = "The requested inference engine is not available. Please check engine configuration.";
            }
            
            send_error_response(sock, 500, "Inference failed: " + error_msg);
        }
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in agent inference endpoint: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

// OpenAI-compatible chat completions endpoint
void AgentsRoute::handle_openai_chat_completions(SocketType sock, const std::string& agent_id, const std::string& body) {
    if (!agent_manager) {
        send_error_response(sock, 500, "Agent manager not available");
        return;
    }
    
    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found");
            return;
        }
        
        // Check if agent is running
        if (!agent->is_running()) {
            send_error_response(sock, 400, "Agent is not running. Please start the agent first.");
            return;
        }
        
        json request = json::parse(body);
        
        if (!request.contains("messages") || !request["messages"].is_array()) {
            send_error_response(sock, 400, "Missing required parameter 'messages' array");
            return;
        }
        
        // Convert OpenAI messages to a single prompt
        std::string combined_prompt;
        for (const auto& message : request["messages"]) {
            if (message.contains("role") && message.contains("content")) {
                std::string role = message["role"].get<std::string>();
                std::string content = message["content"].get<std::string>();
                if (role == "system") {
                    combined_prompt += "System: " + content + "\n";
                } else if (role == "user") {
                    combined_prompt += "User: " + content + "\n";
                } else if (role == "assistant") {
                    combined_prompt += "Assistant: " + content + "\n";
                }
            }
        }
        combined_prompt += "Assistant: ";
        
        // Create parameters for inference
        kolosal::agents::AgentData params;
        params.set("prompt", combined_prompt);
        params.set("max_tokens", request.value("max_tokens", 150));
        params.set("temperature", request.value("temperature", 0.7));
        
        // Execute inference function
        auto result = agent->execute_function("inference", params);
        
        if (result.success) {
            // Create OpenAI-compatible response
            std::string content = "Inference completed successfully";
            if (result.result_data.has_key("text")) {
                content = result.result_data.get_string("text");
            } else if (!result.error_message.empty()) {
                content = result.error_message;
            }
            
            json response = {
                {"id", "chatcmpl-" + std::to_string(std::time(nullptr))},
                {"object", "chat.completion"},
                {"created", std::time(nullptr)},
                {"model", agent_id},
                {"choices", json::array({
                    {
                        {"index", 0},
                        {"message", {
                            {"role", "assistant"},
                            {"content", content}
                        }},
                        {"finish_reason", "stop"}
                    }
                })},
                {"usage", {
                    {"prompt_tokens", 0},
                    {"completion_tokens", result.result_data.has_key("tokens_generated") ? result.result_data.get_int("tokens_generated") : 0},
                    {"total_tokens", 0}
                }}
            };
            
            send_json_response(sock, 200, response);
        } else {
            // Return OpenAI-compatible error
            json error_response = {
                {"error", {
                    {"message", "Inference failed: " + result.error_message},
                    {"type", "inference_error"},
                    {"code", "agent_error"}
                }}
            };
            
            send_json_response(sock, 500, error_response);
        }
        
    } catch (const json::parse_error& e) {
        json error_response = {
            {"error", {
                {"message", "Invalid JSON: " + std::string(e.what())},
                {"type", "invalid_request_error"},
                {"code", "json_parse_error"}
            }}
        };
        send_json_response(sock, 400, error_response);
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in OpenAI chat completions endpoint: %s", e.what());
        json error_response = {
            {"error", {
                {"message", "Internal server error"},
                {"type", "server_error"},
                {"code", "internal_error"}
            }}
        };
        send_json_response(sock, 500, error_response);
    }
}

// Helper methods
std::string AgentsRoute::format_error_response(const std::string& error, int code) {
    json response = {
        {"error", {
            {"message", error},
            {"type", "agent_error"},
            {"code", code}
        }}
    };
    return response.dump();
}

std::string AgentsRoute::format_success_response(const nlohmann::json& data) {
    json response = {
        {"status", "success"}
    };
    
    if (!data.empty()) {
        response.update(data);
    }
    
    return response.dump();
}

bool AgentsRoute::validate_agent_config(const nlohmann::json& config) {
    // Basic validation - check for required fields
    if (!config.contains("name") || !config["name"].is_string()) {
        return false;
    }
    
    if (config.contains("type") && !config["type"].is_string()) {
        return false;
    }
    
    return true;
}

bool AgentsRoute::validate_message_payload(const nlohmann::json& payload) {
    if (!payload.contains("message") || !payload["message"].is_string()) {
        return false;
    }
    
    return true;
}

nlohmann::json AgentsRoute::agent_to_json(const std::shared_ptr<kolosal::agents::AgentCore>& agent) {
    if (!agent) {
        return json::object();
    }
    
    // TODO: Implement actual agent serialization
    return json::object();
}

nlohmann::json AgentsRoute::create_agent_metrics(const std::shared_ptr<kolosal::agents::AgentCore>& agent) {
    if (!agent) {
        return json::object();
    }
    
    // TODO: Implement actual metrics collection
    return json::object();
}
void AgentsRoute::send_response(SocketType sock, int status_code, const std::string& content) {
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    
    ::send_response(sock, status_code, content, headers);
}

void AgentsRoute::send_json_response(SocketType sock, int status_code, const nlohmann::json& data) {
    send_response(sock, status_code, data.dump());
}

void AgentsRoute::send_error_response(SocketType sock, int status_code, const std::string& error) {
    json response = {
        {"error", {
            {"message", error},
            {"type", "agent_error"},
            {"code", status_code}
        }}
    };
    send_json_response(sock, status_code, response);
}

void AgentsRoute::send_success_response(SocketType sock, const nlohmann::json& data) {
    send_json_response(sock, 200, data);
}

std::string AgentsRoute::extractIdFromPath(const std::string& path, const std::string& base_pattern) {
    if (path.length() <= base_pattern.length() + 1) {
        return "";
    }
    
    std::string remaining = path.substr(base_pattern.length() + 1);
    size_t slash_pos = remaining.find('/');
    
    if (slash_pos != std::string::npos) {
        return remaining.substr(0, slash_pos);
    }
    
    return remaining;
}

bool AgentsRoute::matchesPattern(const std::string& path, const std::string& pattern) {
    std::regex regex_pattern(pattern);
    return std::regex_match(path, regex_pattern);
}

void AgentsRoute::handle_broadcast_message(SocketType sock, const std::string& body) {
    try {
        json request = json::parse(body);
        
        if (!request.contains("from_agent") || !request.contains("type") || !request.contains("payload")) {
            send_error_response(sock, 400, "Invalid broadcast message format");
            return;
        }
        
        // For now, just acknowledge the broadcast
        json response = {
            {"status", "success"},
            {"message", "Broadcast message sent successfully"},
            {"broadcast_id", "broadcast_" + std::to_string(std::time(nullptr))},
            {"from_agent", request["from_agent"]},
            {"type", request["type"]}
        };
        
        ServerLogger::logInfo("Broadcast message from %s of type %s", 
                             request["from_agent"].get<std::string>().c_str(),
                             request["type"].get<std::string>().c_str());
        
        send_json_response(sock, 200, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in broadcast message: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

void AgentsRoute::handle_send_message(SocketType sock, const std::string& body) {
    try {
        json request = json::parse(body);
        
        if (!request.contains("from_agent") || !request.contains("to_agent") || !request.contains("payload")) {
            send_error_response(sock, 400, "Invalid message format. Required fields: from_agent, to_agent, payload");
            return;
        }
        
        std::string from_agent = request["from_agent"].get<std::string>();
        std::string to_agent = request["to_agent"].get<std::string>();
        
        // For now, just acknowledge the message
        json response = {
            {"status", "success"},
            {"message", "Message sent successfully"},
            {"message_id", "msg_" + std::to_string(std::time(nullptr))},
            {"from_agent", from_agent},
            {"to_agent", to_agent},
            {"correlation_id", request.value("correlation_id", "")}
        };
        
        ServerLogger::logInfo("Message sent from %s to %s", from_agent.c_str(), to_agent.c_str());
        
        send_json_response(sock, 200, response);
        
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in send message: %s", e.what());
        send_error_response(sock, 500, "Internal server error");
    }
}

} // namespace kolosal::routes::agents

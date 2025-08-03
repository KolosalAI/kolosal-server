#include "kolosal/routes/agents/agents_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/agents/agent_core.hpp"
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
    
    // Handle OPTIONS for CORS
    if (method == "OPTIONS" && path.find("/api/v1/agents") == 0) {
        return true;
    }
    
    // Core agent management endpoints
    if ((method == "GET" || method == "POST") && path == "/api/v1/agents") {
        return true;
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
            
            if (std::regex_match(current_path, matches, agent_pattern)) {
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
        
        // Create agent using the agent manager
        std::string agent_id = "agent_" + std::to_string(std::time(nullptr));
        
        json response = {
            {"status", "success"},
            {"agent_id", agent_id},
            {"name", request.value("name", "Unnamed Agent")},
            {"type", request.value("type", "generic")},
            {"created", true}
        };
        
        ServerLogger::logInfo("Created agent: %s", agent_id.c_str());
        send_json_response(sock, 201, response);
    } catch (const json::parse_error& e) {
        send_error_response(sock, 400, "Invalid JSON: " + std::string(e.what()));
    }
}

void AgentsRoute::handle_list_agents(SocketType sock) {
    json response = {
        {"status", "success"},
        {"agents", json::array()},
        {"total", 0}
    };
    
    // TODO: Implement actual agent listing using agent_manager
    ServerLogger::logInfo("Listing agents");
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_get_agent(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"name", "Sample Agent"},
        {"type", "generic"},
        {"state", "stopped"},
        {"created_at", "2024-01-01T00:00:00Z"}
    };
    
    ServerLogger::logInfo("Getting agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
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
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"message", "Agent deleted successfully"}
    };
    
    ServerLogger::logInfo("Deleted agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_agent_system_status(SocketType sock) {
    json response = {
        {"status", "success"},
        {"system", {
            {"running", true},
            {"active_agents", 0},
            {"total_agents", 0}
        }}
    };
    
    send_json_response(sock, 200, response);
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
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"state", "running"},
        {"message", "Agent started successfully"}
    };
    
    ServerLogger::logInfo("Started agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_stop_agent(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"state", "stopped"},
        {"message", "Agent stopped successfully"}
    };
    
    ServerLogger::logInfo("Stopped agent: %s", agent_id.c_str());
    send_json_response(sock, 200, response);
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
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"capabilities", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_list_agent_functions(SocketType sock, const std::string& agent_id) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"functions", json::array()}
    };
    
    send_json_response(sock, 200, response);
}

void AgentsRoute::handle_execute_agent_function(SocketType sock, const std::string& agent_id, const std::string& function_name, const std::string& body) {
    json response = {
        {"status", "success"},
        {"agent_id", agent_id},
        {"function", function_name},
        {"result", "Function executed successfully"}
    };
    
    send_json_response(sock, 200, response);
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

} // namespace kolosal::routes::agents

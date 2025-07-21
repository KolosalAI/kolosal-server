#include "kolosal/routes/collaboration_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

CollaborationRoute::CollaborationRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
}

CollaborationRoute::~CollaborationRoute() = default;

bool CollaborationRoute::match(const std::string& method, const std::string& path) {
    // Store current request context
    current_method = method;
    current_path = path;
    
    // Match collaboration endpoints
    return (path.find("/api/v1/collaboration/") == 0 ||
            path.find("/api/v1/communication/") == 0);
}

void CollaborationRoute::handle(SocketType sock, const std::string& body) {
    try {
        const std::string& path = current_path;
        const std::string& method = current_method;
        
        // Route to appropriate handler
        if (path.find("/api/v1/collaboration/groups") == 0) {
            handleCollaborationGroups(sock, body);
        } else if (path.find("/api/v1/communication/direct") == 0) {
            handleDirectCommunication(sock, body);
        } else if (path.find("/api/v1/communication/broadcast") == 0) {
            handleBroadcastCommunication(sock, body);
        } else {
            sendErrorResponse(sock, 404, "Collaboration endpoint not found");
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in collaboration route: %s", e.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(e.what()));
    }
}

void CollaborationRoute::handleCollaborationGroups(SocketType sock, const std::string& body) {
    if (!agent_manager) {
        sendErrorResponse(sock, 503, "Agent manager not available");
        return;
    }
    
    const std::string& method = current_method;
    const std::string& path = current_path;
    
    try {
        if (method == "POST" && path == "/api/v1/collaboration/groups") {
            // Create collaboration group
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                sendErrorResponse(sock, 400, "Invalid JSON format");
                return;
            }
            
            // Extract group details
            std::string group_name = request_data.value("name", "");
            auto agent_ids = request_data.value("agent_ids", json::array());
            
            if (group_name.empty()) {
                sendErrorResponse(sock, 400, "Missing group name");
                return;
            }
            
            if (agent_ids.empty()) {
                sendErrorResponse(sock, 400, "Missing agent_ids");
                return;
            }
            
            // Validate that all agents exist
            for (const auto& agent_id : agent_ids) {
                auto agent = agent_manager->get_agent(agent_id);
                if (!agent) {
                    sendErrorResponse(sock, 400, "Agent not found: " + agent_id.get<std::string>());
                    return;
                }
            }
            
            // Create the collaboration group (simplified implementation)
            json response_data = {
                {"group_id", "group_" + std::to_string(std::time(nullptr))},
                {"name", group_name},
                {"agent_ids", agent_ids},
                {"status", "created"},
                {"created_at", std::time(nullptr)}
            };
            
            sendSuccessResponse(sock, response_data);
            
        } else if (method == "GET" && path == "/api/v1/collaboration/groups") {
            // List collaboration groups
            json groups = json::array();
            
            // Return empty list for now (simplified implementation)
            json response_data = {
                {"groups", groups},
                {"total", 0}
            };
            
            sendSuccessResponse(sock, response_data);
            
        } else {
            // Handle group-specific operations
            std::string group_id = extractGroupId(path);
            if (!group_id.empty()) {
                if (method == "GET") {
                    // Get group details
                    json response_data = {
                        {"group_id", group_id},
                        {"name", "Sample Group"},
                        {"agent_ids", json::array()},
                        {"status", "active"}
                    };
                    sendSuccessResponse(sock, response_data);
                } else if (method == "DELETE") {
                    // Delete group
                    json response_data = {
                        {"group_id", group_id},
                        {"status", "deleted"}
                    };
                    sendSuccessResponse(sock, response_data);
                } else {
                    sendErrorResponse(sock, 405, "Method not allowed");
                }
            } else {
                sendErrorResponse(sock, 404, "Group not found");
            }
        }
    } catch (const std::exception& e) {
        sendErrorResponse(sock, 500, "Failed to handle collaboration groups: " + std::string(e.what()));
    }
}

void CollaborationRoute::handleDirectCommunication(SocketType sock, const std::string& body) {
    if (!agent_manager) {
        sendErrorResponse(sock, 503, "Agent manager not available");
        return;
    }
    
    try {
        // Parse the JSON body
        json request_data;
        try {
            request_data = json::parse(body);
        } catch (const json::parse_error& e) {
            sendErrorResponse(sock, 400, "Invalid JSON format");
            return;
        }
        
        // Extract required fields
        std::string from_agent = request_data.value("from_agent", "");
        std::string to_agent = request_data.value("to_agent", "");
        std::string message = request_data.value("message", "");
        
        if (from_agent.empty() || to_agent.empty() || message.empty()) {
            sendErrorResponse(sock, 400, "Missing required fields: from_agent, to_agent, message");
            return;
        }
        
        // Validate agents exist
        auto from_agent_obj = agent_manager->get_agent(from_agent);
        auto to_agent_obj = agent_manager->get_agent(to_agent);
        
        if (!from_agent_obj) {
            sendErrorResponse(sock, 404, "From agent not found");
            return;
        }
        if (!to_agent_obj) {
            sendErrorResponse(sock, 404, "To agent not found");
            return;
        }
        
        // Simulate message delivery
        json response_data = {
            {"success", true},
            {"message_id", "msg_" + std::to_string(std::time(nullptr))},
            {"from_agent", from_agent},
            {"to_agent", to_agent},
            {"message", message},
            {"status", "delivered"},
            {"timestamp", std::time(nullptr)}
        };
        
        sendSuccessResponse(sock, response_data);
        
    } catch (const std::exception& e) {
        sendErrorResponse(sock, 500, "Failed to handle direct communication: " + std::string(e.what()));
    }
}

void CollaborationRoute::handleBroadcastCommunication(SocketType sock, const std::string& body) {
    if (!agent_manager) {
        sendErrorResponse(sock, 503, "Agent manager not available");
        return;
    }
    
    try {
        // Parse the JSON body
        json request_data;
        try {
            request_data = json::parse(body);
        } catch (const json::parse_error& e) {
            sendErrorResponse(sock, 400, "Invalid JSON format");
            return;
        }
        
        // Extract required fields
        std::string from_agent = request_data.value("from_agent", "");
        std::string message = request_data.value("message", "");
        auto target_agents = request_data.value("target_agents", json::array());
        
        if (from_agent.empty() || message.empty()) {
            sendErrorResponse(sock, 400, "Missing required fields: from_agent, message");
            return;
        }
        
        // Validate from agent exists
        auto from_agent_obj = agent_manager->get_agent(from_agent);
        if (!from_agent_obj) {
            sendErrorResponse(sock, 404, "From agent not found");
            return;
        }
        
        // If no target agents specified, broadcast to all
        if (target_agents.empty()) {
            auto all_agents = agent_manager->list_agents();
            for (const auto& agent_id : all_agents) {
                if (agent_id != from_agent) {
                    target_agents.push_back(agent_id);
                }
            }
        }
        
        // Validate target agents exist
        for (const auto& agent_id : target_agents) {
            auto agent = agent_manager->get_agent(agent_id);
            if (!agent) {
                sendErrorResponse(sock, 400, "Target agent not found: " + agent_id.get<std::string>());
                return;
            }
        }
        
        // Simulate broadcast delivery
        json response_data = {
            {"success", true},
            {"broadcast_id", "broadcast_" + std::to_string(std::time(nullptr))},
            {"from_agent", from_agent},
            {"target_agents", target_agents},
            {"message", message},
            {"status", "delivered"},
            {"delivery_count", target_agents.size()},
            {"timestamp", std::time(nullptr)}
        };
        
        sendSuccessResponse(sock, response_data);
        
    } catch (const std::exception& e) {
        sendErrorResponse(sock, 500, "Failed to handle broadcast communication: " + std::string(e.what()));
    }
}

std::string CollaborationRoute::extractGroupId(const std::string& path) {
    // Pattern: /api/v1/collaboration/groups/{group_id}
    std::regex group_pattern(R"(/api/v1/collaboration/groups/([^/]+))");
    std::smatch matches;
    if (std::regex_search(path, matches, group_pattern)) {
        return matches[1].str();
    }
    return "";
}

void CollaborationRoute::sendErrorResponse(SocketType sock, int status, const std::string& message, 
                                         const std::string& error_type) {
    json error_response = {
        {"success", false},
        {"error", {
            {"message", message},
            {"type", error_type}
        }}
    };
    
    std::string response_str = error_response.dump();
    send_response(sock, status, response_str);
}

void CollaborationRoute::sendSuccessResponse(SocketType sock, const nlohmann::json& data) {
    json response = {
        {"success", true},
        {"data", data}
    };
    
    std::string response_str = response.dump();
    send_response(sock, 200, response_str);
}

} // namespace kolosal::routes

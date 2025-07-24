#include "kolosal/routes/agent_documents_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

// Agent Document List Route - handles GET /v1/agents/{agent_id}/documents
class AgentDocumentListRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentDocumentsRoute* parent;
    std::string matched_agent_id;

public:
    AgentDocumentListRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentDocumentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "GET") return false;
        
        std::regex pattern(R"(^(?:/v1)?/api/v1/agents/([^/]+)/documents/?$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Mock response for agent documents
            json response_data = {
                {"agent_id", matched_agent_id},
                {"documents", json::array()},
                {"total_count", 0},
                {"message", "Agent documents retrieved successfully"}
            };
            
            send_response(sock, 200, parent->format_success_response(response_data));
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error listing agent documents: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

// Agent Document Add Route - handles POST /v1/agents/{agent_id}/documents
class AgentDocumentAddRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentDocumentsRoute* parent;
    std::string matched_agent_id;

public:
    AgentDocumentAddRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentDocumentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "POST") return false;
        
        std::regex pattern(R"(^(?:/v1)?/api/v1/agents/([^/]+)/documents/?$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Parse the JSON body
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                return;
            }
            
            // Validate required fields
            if (!request_data.contains("content") || !request_data["content"].is_string()) {
                send_response(sock, 400, parent->format_error_response("Missing or invalid 'content' field", 400));
                return;
            }
            
            std::string document_id = "doc_" + std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            
            json response_data = {
                {"agent_id", matched_agent_id},
                {"document_id", document_id},
                {"status", "added"},
                {"message", "Document added to agent successfully"}
            };
            
            send_response(sock, 201, parent->format_success_response(response_data));
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error adding agent document: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

// Agent Document Delete Route - handles DELETE /v1/agents/{agent_id}/documents/{document_id}
class AgentDocumentDeleteRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentDocumentsRoute* parent;
    std::string matched_agent_id;
    std::string matched_document_id;

public:
    AgentDocumentDeleteRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentDocumentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "DELETE") return false;
        
        std::regex pattern(R"(^(?:/v1)?/api/v1/agents/([^/]+)/documents/([^/]+)/?$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            matched_document_id = matches[2].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Mock document deletion
            json response_data = {
                {"agent_id", matched_agent_id},
                {"document_id", matched_document_id},
                {"status", "deleted"},
                {"message", "Document deleted from agent successfully"}
            };
            
            send_response(sock, 200, parent->format_success_response(response_data));
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error deleting agent document: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

// Agent Documents Bulk Delete Route - handles DELETE /v1/agents/{agent_id}/documents
class AgentDocumentsBulkDeleteRoute : public IRoute {
private:
    std::shared_ptr<agents::YAMLConfigurableAgentManager> agent_manager;
    AgentDocumentsRoute* parent;
    std::string matched_agent_id;

public:
    AgentDocumentsBulkDeleteRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager, AgentDocumentsRoute* p) 
        : agent_manager(manager), parent(p) {}
    
    bool match(const std::string& method, const std::string& path) override {
        if (method != "DELETE") return false;
        
        std::regex pattern(R"(^(?:/v1)?/api/v1/agents/([^/]+)/documents/?$)");
        std::smatch matches;
        if (std::regex_match(path, matches, pattern)) {
            matched_agent_id = matches[1].str();
            return true;
        }
        return false;
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            auto agent = agent_manager->get_agent(matched_agent_id);
            
            if (!agent) {
                send_response(sock, 404, parent->format_error_response("Agent not found", 404));
                return;
            }
            
            // Parse the JSON body to get document IDs to delete
            json request_data;
            if (!body.empty()) {
                try {
                    request_data = json::parse(body);
                } catch (const json::parse_error& e) {
                    send_response(sock, 400, parent->format_error_response("Invalid JSON format", 400));
                    return;
                }
            }
            
            int deleted_count = 0;
            if (request_data.contains("document_ids") && request_data["document_ids"].is_array()) {
                deleted_count = request_data["document_ids"].size();
            }
            
            json response_data = {
                {"agent_id", matched_agent_id},
                {"deleted_count", deleted_count},
                {"status", "deleted"},
                {"message", "Documents deleted from agent successfully"}
            };
            
            send_response(sock, 200, parent->format_success_response(response_data));
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Error bulk deleting agent documents: %s", e.what());
            send_response(sock, 500, parent->format_error_response(e.what()));
        }
    }
};

KOLOSAL_SERVER_API AgentDocumentsRoute::AgentDocumentsRoute(std::shared_ptr<agents::YAMLConfigurableAgentManager> manager)
    : agent_manager(manager) {
}

void KOLOSAL_SERVER_API AgentDocumentsRoute::setup_routes(Server& server) {
    // Register individual route handlers
    server.addRoute(std::make_unique<AgentDocumentListRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentDocumentAddRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentDocumentDeleteRoute>(agent_manager, this));
    server.addRoute(std::make_unique<AgentDocumentsBulkDeleteRoute>(agent_manager, this));
}

// Helper methods
std::string AgentDocumentsRoute::format_error_response(const std::string& error, int code) {
    json response = {
        {"status", "error"},
        {"error", {
            {"message", error},
            {"code", code}
        }}
    };
    return response.dump();
}

std::string AgentDocumentsRoute::format_success_response(const nlohmann::json& data) {
    json response = {
        {"status", "success"},
        {"data", data}
    };
    return response.dump();
}

} // namespace kolosal::routes

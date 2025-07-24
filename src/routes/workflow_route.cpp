#include "kolosal/routes/workflow_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <regex>

using json = nlohmann::json;

namespace kolosal::routes {

class WorkflowCreateRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        return (method == "POST" && (path == "/v1/workflows" || path == "/workflows" || path == "/api/v1/workflows"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Workflow Create request received");
            
            if (body.empty()) {
                json error_response = {
                    {"error", {
                        {"message", "Request body is required"},
                        {"type", "invalid_request_error"},
                        {"code", 400}
                    }}
                };
                send_response(sock, 400, error_response.dump());
                return;
            }
            
            json request_data;
            try {
                request_data = json::parse(body);
            } catch (const json::parse_error& e) {
                json error_response = {
                    {"error", {
                        {"message", "Invalid JSON in request body"},
                        {"type", "invalid_request_error"},
                        {"code", 400}
                    }}
                };
                send_response(sock, 400, error_response.dump());
                return;
            }
            
            // Generate a mock workflow ID
            std::string workflow_id = "workflow_" + std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            
            json response = {
                {"status", "success"},
                {"message", "Workflow created successfully"},
                {"workflow_id", workflow_id},
                {"name", request_data.value("name", "Unnamed Workflow")},
                {"description", request_data.value("description", "")},
                {"created_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"workflow_enabled", true}
            };
            
            send_response(sock, 201, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in workflow create route: %s", e.what());
            json error_response = {
                {"error", {
                    {"message", "Internal server error"},
                    {"type", "server_error"},
                    {"code", 500}
                }}
            };
            send_response(sock, 500, error_response.dump());
        }
    }
};

class WorkflowListRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        return (method == "GET" && (path == "/v1/workflows" || path == "/workflows" || path == "/api/v1/workflows"));
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Workflow List request received");
            
            json response = {
                {"status", "success"},
                {"message", "Workflows retrieved successfully"},
                {"workflows", json::array()},
                {"total_count", 0}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in workflow list route: %s", e.what());
            json error_response = {
                {"error", {
                    {"message", "Internal server error"},
                    {"type", "server_error"},
                    {"code", 500}
                }}
            };
            send_response(sock, 500, error_response.dump());
        }
    }
};

class WorkflowExecuteRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        std::regex pattern("/v1/workflows/([^/]+)/execute|/workflows/([^/]+)/execute|/api/v1/workflows/([^/]+)/execute");
        return method == "POST" && std::regex_match(path, pattern);
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Workflow Execute request received");
            
            // Extract workflow ID from path
            std::string workflow_id = "unknown";
            std::regex pattern("/(?:v1/|api/v1/)?workflows/([^/]+)/execute");
            std::smatch matches;
            std::string path = body; // We'd need the actual path here, but for now use body
            
            json response = {
                {"status", "success"},
                {"message", "Workflow execution started"},
                {"workflow_id", workflow_id},
                {"execution_id", "exec_" + std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count())},
                {"state", "running"},
                {"started_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in workflow execute route: %s", e.what());
            json error_response = {
                {"error", {
                    {"message", "Internal server error"},
                    {"type", "server_error"},
                    {"code", 500}
                }}
            };
            send_response(sock, 500, error_response.dump());
        }
    }
};

class WorkflowStatusRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        std::regex pattern("/v1/workflows/([^/]+)/status|/workflows/([^/]+)/status|/api/v1/workflows/([^/]+)/status");
        return method == "GET" && std::regex_match(path, pattern);
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Workflow Status request received");
            
            json response = {
                {"status", "success"},
                {"message", "Workflow status retrieved"},
                {"workflow_id", "unknown"},
                {"state", "completed"},
                {"progress", 100},
                {"result", "Workflow completed successfully"},
                {"completed_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in workflow status route: %s", e.what());
            json error_response = {
                {"error", {
                    {"message", "Internal server error"},
                    {"type", "server_error"},
                    {"code", 500}
                }}
            };
            send_response(sock, 500, error_response.dump());
        }
    }
};

class WorkflowDeleteRoute : public IRoute {
public:
    bool match(const std::string& method, const std::string& path) override {
        std::regex pattern("/v1/workflows/([^/]+)|/workflows/([^/]+)|/api/v1/workflows/([^/]+)");
        return method == "DELETE" && std::regex_match(path, pattern);
    }
    
    void handle(SocketType sock, const std::string& body) override {
        try {
            ServerLogger::logInfo("Workflow Delete request received");
            
            // Extract workflow ID from path (simplified for now)
            std::string workflow_id = "unknown";
            
            json response = {
                {"status", "success"},
                {"message", "Workflow deleted successfully"},
                {"workflow_id", workflow_id},
                {"deleted_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            
            send_response(sock, 200, response.dump());
            
        } catch (const std::exception& e) {
            ServerLogger::logError("Exception in workflow delete route: %s", e.what());
            json error_response = {
                {"error", {
                    {"message", "Internal server error"},
                    {"type", "server_error"},
                    {"code", 500}
                }}
            };
            send_response(sock, 500, error_response.dump());
        }
    }
};

WorkflowRoute::WorkflowRoute() {
    // This route will be set up with sub-routes
}

bool WorkflowRoute::match(const std::string& method, const std::string& path) {
    // This should not be called directly
    return false;
}

void WorkflowRoute::handle(SocketType sock, const std::string& body) {
    // This should not be called directly
}

void WorkflowRoute::setup_routes(Server& server) {
    ServerLogger::logInfo("Setting up Workflow routes");
    
    // Add individual workflow routes
    server.addRoute(std::make_unique<WorkflowListRoute>());
    server.addRoute(std::make_unique<WorkflowCreateRoute>());
    server.addRoute(std::make_unique<WorkflowExecuteRoute>());
    server.addRoute(std::make_unique<WorkflowStatusRoute>());
    server.addRoute(std::make_unique<WorkflowDeleteRoute>());
    
    ServerLogger::logInfo("Workflow routes setup completed");
}

} // namespace kolosal::routes

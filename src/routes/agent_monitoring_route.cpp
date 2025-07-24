#include "kolosal/routes/agent_monitoring_route.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <sstream>
#include <chrono>
#include <iomanip>
#ifdef _WIN32
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

using json = nlohmann::json;

namespace kolosal {

AgentMonitoringRoute::AgentMonitoringRoute(
    std::shared_ptr<agents::YAMLConfigurableAgentManager> manager,
    std::shared_ptr<agents::AgentOrchestrator> orchestrator
) : agent_manager(manager), agent_orchestrator(orchestrator) {
}

bool AgentMonitoringRoute::match(const std::string& method, const std::string& path) {
    return (method == "GET" && (
        // Old style paths
        path == "/agents/health" ||
        path == "/agents/metrics" ||
        path == "/agents/system-metrics" ||
        path == "/agents/orchestrator/status" ||
        path == "/agents/workflows/metrics" ||
        // New API v1 style paths
        path == "/api/v1/agents/health" ||
        path == "/api/v1/agents/metrics" ||
        path == "/api/v1/agents/system/status" ||
        path == "/api/v1/agents/system/metrics" ||
        path == "/api/v1/orchestration/status" ||
        path == "/api/v1/orchestration/metrics" ||
        // Global metrics endpoints
        path == "/metrics" ||
        path == "/completion-metrics" ||
        // Individual agent status paths (both old and new style)
        (path.find("/agents/") == 0 && path.find("/status") != std::string::npos) ||
        (path.find("/api/v1/agents/") == 0 && path.find("/status") != std::string::npos)
    ));
}

void AgentMonitoringRoute::handle(SocketType sock, const std::string& body) {
    try {
        std::string path;
        std::string method;
        
        // Extract path and method from the request
        std::istringstream request_stream(body);
        std::string request_line;
        if (std::getline(request_stream, request_line)) {
            std::istringstream line_stream(request_line);
            line_stream >> method >> path;
        }

        ServerLogger::logInfo("Agent monitoring request: %s %s", method.c_str(), path.c_str());

        // Handle old style paths
        if (path == "/agents/health") {
            handle_agent_health(sock);
        } else if (path == "/agents/metrics") {
            handle_agent_metrics(sock);
        } else if (path == "/agents/system-metrics") {
            handle_system_metrics(sock);
        } else if (path == "/agents/orchestrator/status") {
            handle_orchestrator_status(sock);
        } else if (path == "/agents/workflows/metrics") {
            handle_workflow_metrics(sock);
        }
        // Handle new API v1 style paths
        else if (path == "/api/v1/agents/health") {
            handle_agent_health(sock);
        } else if (path == "/api/v1/agents/metrics") {
            handle_agent_metrics(sock);
        } else if (path == "/api/v1/agents/system/status") {
            handle_agent_health(sock);
        } else if (path == "/api/v1/agents/system/metrics") {
            handle_system_metrics(sock);
        } else if (path == "/api/v1/orchestration/status") {
            handle_orchestrator_status(sock);
        } else if (path == "/api/v1/orchestration/metrics") {
            handle_workflow_metrics(sock);
        }
        // Handle global metrics endpoints
        else if (path == "/metrics") {
            handle_system_metrics(sock);
        } else if (path == "/completion-metrics") {
            handle_completion_metrics(sock);
        }
        // Handle individual agent status paths (both old and new style)
        else if ((path.find("/agents/") == 0 || path.find("/api/v1/agents/") == 0) && path.find("/status") != std::string::npos) {
            std::string agent_id = extract_agent_id_from_path(path);
            handle_agent_status(sock, agent_id);
        } else {
            send_error_response(sock, 404, "Agent monitoring endpoint not found");
        }
    } catch (const std::exception& e) {
        ServerLogger::logError("Error in agent monitoring route: %s", e.what());
        send_error_response(sock, 500, "Internal server error: " + std::string(e.what()));
    }
}

void AgentMonitoringRoute::handle_agent_health(SocketType sock) {
    if (!agent_manager) {
        send_error_response(sock, 503, "Agent manager not available");
        return;
    }

    try {
        std::ostringstream json_response;
        json_response << "{\n";
        json_response << "  \"status\": \"healthy\",\n";
        json_response << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
        json_response << "  \"agent_manager\": {\n";
        json_response << "    \"running\": " << (agent_manager->is_running() ? "true" : "false") << ",\n";
        
        auto agent_ids = agent_manager->list_agents();
        json_response << "    \"total_agents\": " << agent_ids.size() << ",\n";
        
        int healthy_agents = 0;
        int unhealthy_agents = 0;
        
        json_response << "    \"agents\": [\n";
        for (size_t i = 0; i < agent_ids.size(); ++i) {
            try {
                auto agent = agent_manager->get_agent(agent_ids[i]);
                bool is_healthy = agent && agent->is_running();
                if (is_healthy) healthy_agents++;
                else unhealthy_agents++;
                
                json_response << "      {\n";
                json_response << "        \"id\": \"" << agent_ids[i] << "\",\n";
                json_response << "        \"status\": \"" << (is_healthy ? "healthy" : "unhealthy") << "\"\n";
                json_response << "      }";
                if (i < agent_ids.size() - 1) json_response << ",";
                json_response << "\n";
            } catch (const std::exception& e) {
                unhealthy_agents++;
                json_response << "      {\n";
                json_response << "        \"id\": \"" << agent_ids[i] << "\",\n";
                json_response << "        \"status\": \"error\",\n";
                json_response << "        \"error\": \"" << e.what() << "\"\n";
                json_response << "      }";
                if (i < agent_ids.size() - 1) json_response << ",";
                json_response << "\n";
            }
        }
        json_response << "    ],\n";
        
        json_response << "    \"healthy_agents\": " << healthy_agents << ",\n";
        json_response << "    \"unhealthy_agents\": " << unhealthy_agents << "\n";
        json_response << "  }";
        
        if (agent_orchestrator) {
            json_response << ",\n";
            json_response << "  \"orchestrator\": {\n";
            json_response << "    \"running\": " << (agent_orchestrator->is_running() ? "true" : "false") << ",\n";
            auto orch_metrics = agent_orchestrator->get_orchestration_metrics();
            json_response << "    \"active_workflows\": " << orch_metrics.at("active_workflows") << ",\n";
            json_response << "    \"completed_workflows\": " << orch_metrics.at("completed_workflows") << ",\n";
            json_response << "    \"failed_workflows\": " << orch_metrics.at("failed_workflows") << "\n";
            json_response << "  }";
        }
        
        json_response << "\n}";
        
        send_json_response(sock, 200, json_response.str());
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get agent health: " + std::string(e.what()));
    }
}

void AgentMonitoringRoute::handle_agent_metrics(SocketType sock) {
    if (!agent_manager) {
        send_error_response(sock, 503, "Agent manager not available");
        return;
    }

    try {
        std::ostringstream json_response;
        json_response << "{\n";
        json_response << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
        json_response << "  \"system_metrics\": {\n";
        
        auto agent_ids = agent_manager->list_agents();
        json_response << "    \"total_agents\": " << agent_ids.size() << ",\n";
        
        // Collect detailed metrics per agent
        json_response << "    \"agent_details\": [\n";
        for (size_t i = 0; i < agent_ids.size(); ++i) {
            try {
                auto agent = agent_manager->get_agent(agent_ids[i]);
                if (agent) {
                    json_response << "      {\n";
                    json_response << "        \"id\": \"" << agent_ids[i] << "\",\n";
                    json_response << "        \"name\": \"" << agent->get_agent_name() << "\",\n";
                    json_response << "        \"type\": \"" << agent->get_agent_type() << "\",\n";
                    json_response << "        \"running\": " << (agent->is_running() ? "true" : "false") << ",\n";
                    
                    auto capabilities = agent->get_capabilities();
                    json_response << "        \"capabilities\": [";
                    for (size_t j = 0; j < capabilities.size(); ++j) {
                        json_response << "\"" << capabilities[j] << "\"";
                        if (j < capabilities.size() - 1) json_response << ", ";
                    }
                    json_response << "]\n";
                    json_response << "      }";
                } else {
                    json_response << "      {\n";
                    json_response << "        \"id\": \"" << agent_ids[i] << "\",\n";
                    json_response << "        \"status\": \"not_found\"\n";
                    json_response << "      }";
                }
                if (i < agent_ids.size() - 1) json_response << ",";
                json_response << "\n";
            } catch (const std::exception& e) {
                json_response << "      {\n";
                json_response << "        \"id\": \"" << agent_ids[i] << "\",\n";
                json_response << "        \"status\": \"error\",\n";
                json_response << "        \"error\": \"" << e.what() << "\"\n";
                json_response << "      }";
                if (i < agent_ids.size() - 1) json_response << ",";
                json_response << "\n";
            }
        }
        json_response << "    ]\n";
        json_response << "  }\n";
        json_response << "}";
        
        send_json_response(sock, 200, json_response.str());
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get agent metrics: " + std::string(e.what()));
    }
}

void AgentMonitoringRoute::handle_agent_status(SocketType sock, const std::string& agent_id) {
    if (!agent_manager) {
        send_error_response(sock, 503, "Agent manager not available");
        return;
    }

    if (agent_id.empty()) {
        send_error_response(sock, 400, "Agent ID is required");
        return;
    }

    try {
        auto agent = agent_manager->get_agent(agent_id);
        if (!agent) {
            send_error_response(sock, 404, "Agent not found: " + agent_id);
            return;
        }

        std::ostringstream json_response;
        json_response << "{\n";
        json_response << "  \"id\": \"" << agent_id << "\",\n";
        json_response << "  \"name\": \"" << agent->get_agent_name() << "\",\n";
        json_response << "  \"type\": \"" << agent->get_agent_type() << "\",\n";
        json_response << "  \"running\": " << (agent->is_running() ? "true" : "false") << ",\n";
        json_response << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
        
        auto capabilities = agent->get_capabilities();
        json_response << "  \"capabilities\": [";
        for (size_t i = 0; i < capabilities.size(); ++i) {
            json_response << "\"" << capabilities[i] << "\"";
            if (i < capabilities.size() - 1) json_response << ", ";
        }
        json_response << "]\n";
        json_response << "}";
        
        send_json_response(sock, 200, json_response.str());
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get agent status: " + std::string(e.what()));
    }
}

void AgentMonitoringRoute::handle_system_metrics(SocketType sock) {
    std::ostringstream json_response;
    json_response << "{\n";
    json_response << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
    json_response << "  \"agent_system\": {\n";
    
    if (agent_manager) {
        json_response << "    \"manager_status\": \"" << (agent_manager->is_running() ? "running" : "stopped") << "\",\n";
        auto agent_ids = agent_manager->list_agents();
        json_response << "    \"total_agents\": " << agent_ids.size();
    } else {
        json_response << "    \"manager_status\": \"not_available\",\n";
        json_response << "    \"total_agents\": 0";
    }
    
    if (agent_orchestrator) {
        json_response << ",\n";
        json_response << "    \"orchestrator_status\": \"" << (agent_orchestrator->is_running() ? "running" : "stopped") << "\",\n";
        auto metrics = agent_orchestrator->get_orchestration_metrics();
        json_response << "    \"orchestrator_metrics\": {\n";
        json_response << "      \"active_workflows\": " << metrics.at("active_workflows") << ",\n";
        json_response << "      \"completed_workflows\": " << metrics.at("completed_workflows") << ",\n";
        json_response << "      \"failed_workflows\": " << metrics.at("failed_workflows") << "\n";
        json_response << "    }";
    } else {
        json_response << ",\n";
        json_response << "    \"orchestrator_status\": \"not_available\"";
    }
    
    json_response << "\n  }\n";
    json_response << "}";
    
    send_json_response(sock, 200, json_response.str());
}

void AgentMonitoringRoute::handle_orchestrator_status(SocketType sock) {
    if (!agent_orchestrator) {
        send_error_response(sock, 503, "Agent orchestrator not available");
        return;
    }

    try {
        std::ostringstream json_response;
        json_response << "{\n";
        json_response << "  \"status\": \"" << (agent_orchestrator->is_running() ? "running" : "stopped") << "\",\n";
        json_response << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
        
        auto metrics = agent_orchestrator->get_orchestration_metrics();
        json_response << "  \"metrics\": {\n";
        json_response << "    \"active_workflows\": " << metrics.at("active_workflows") << ",\n";
        json_response << "    \"completed_workflows\": " << metrics.at("completed_workflows") << ",\n";
        json_response << "    \"failed_workflows\": " << metrics.at("failed_workflows") << "\n";
        json_response << "  },\n";
        
        auto active_workflows = agent_orchestrator->get_active_workflows();
        json_response << "  \"active_workflows\": [";
        for (size_t i = 0; i < active_workflows.size(); ++i) {
            json_response << "\"" << active_workflows[i] << "\"";
            if (i < active_workflows.size() - 1) json_response << ", ";
        }
        json_response << "]\n";
        json_response << "}";
        
        send_json_response(sock, 200, json_response.str());
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get orchestrator status: " + std::string(e.what()));
    }
}

void AgentMonitoringRoute::handle_workflow_metrics(SocketType sock) {
    if (!agent_orchestrator) {
        send_error_response(sock, 503, "Agent orchestrator not available");
        return;
    }

    try {
        auto metrics = agent_orchestrator->get_orchestration_metrics();
        
        std::ostringstream json_response;
        json_response << "{\n";
        json_response << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
        json_response << "  \"workflow_metrics\": {\n";
        json_response << "    \"total_workflows\": " << (metrics.at("active_workflows") + metrics.at("completed_workflows") + metrics.at("failed_workflows")) << ",\n";
        json_response << "    \"active_workflows\": " << metrics.at("active_workflows") << ",\n";
        json_response << "    \"completed_workflows\": " << metrics.at("completed_workflows") << ",\n";
        json_response << "    \"failed_workflows\": " << metrics.at("failed_workflows") << ",\n";
        json_response << "    \"success_rate\": " << (metrics.at("completed_workflows") + metrics.at("failed_workflows") > 0 ? 
            (double)metrics.at("completed_workflows") / (metrics.at("completed_workflows") + metrics.at("failed_workflows")) * 100.0 : 0.0) << "\n";
        json_response << "  }\n";
        json_response << "}";
        
        send_json_response(sock, 200, json_response.str());
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get workflow metrics: " + std::string(e.what()));
    }
}

void AgentMonitoringRoute::handle_completion_metrics(SocketType sock) {
    try {
        std::ostringstream json_response;
        json_response << "{\n";
        json_response << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
        json_response << "  \"completion_metrics\": {\n";
        json_response << "    \"total_requests\": 0,\n";
        json_response << "    \"successful_requests\": 0,\n";
        json_response << "    \"failed_requests\": 0,\n";
        json_response << "    \"average_response_time\": 0.0,\n";
        json_response << "    \"tokens_generated\": 0,\n";
        json_response << "    \"average_tokens_per_second\": 0.0\n";
        json_response << "  },\n";
        json_response << "  \"engine_metrics\": {\n";
        json_response << "    \"active_engines\": 0,\n";
        json_response << "    \"loaded_engines\": 0,\n";
        json_response << "    \"total_engines\": 0\n";
        json_response << "  }\n";
        json_response << "}";
        
        send_json_response(sock, 200, json_response.str());
    } catch (const std::exception& e) {
        send_error_response(sock, 500, "Failed to get completion metrics: " + std::string(e.what()));
    }
}

std::string AgentMonitoringRoute::extract_agent_id_from_path(const std::string& path) {
    // Extract agent ID from path like "/agents/{agent_id}/status"
    size_t start = path.find("/agents/");
    if (start == std::string::npos) return "";
    
    start += 8; // Length of "/agents/"
    size_t end = path.find("/", start);
    if (end == std::string::npos) return "";
    
    return path.substr(start, end - start);
}

void AgentMonitoringRoute::send_json_response(SocketType sock, int status_code, const std::string& json_content) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " ";
    switch (status_code) {
        case 200: response << "OK"; break;
        case 404: response << "Not Found"; break;
        case 500: response << "Internal Server Error"; break;
        case 503: response << "Service Unavailable"; break;
        default: response << "Unknown"; break;
    }
    response << "\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << json_content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << json_content;
    
    std::string response_str = response.str();
    send(sock, response_str.c_str(), response_str.length(), 0);
}

void AgentMonitoringRoute::send_error_response(SocketType sock, int status_code, const std::string& error_message) {
    std::ostringstream json_error;
    json_error << "{\n";
    json_error << "  \"error\": \"" << error_message << "\",\n";
    json_error << "  \"timestamp\": \"" << std::time(nullptr) << "\"\n";
    json_error << "}";
    
    send_json_response(sock, status_code, json_error.str());
}

} // namespace kolosal

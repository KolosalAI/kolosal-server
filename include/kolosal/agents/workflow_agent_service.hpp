#pragma once

#include "kolosal/agents/agent_interfaces.hpp"
#include <json.hpp>
#include <memory>
#include <future>
#include <vector>
#include <string>
#include <map>

namespace kolosal::agents {

using json = nlohmann::json;

class WorkflowAgentService {
public:
    WorkflowAgentService() = default;
    ~WorkflowAgentService() = default;

    // Workflow management structures
    struct WorkflowRequest {
        std::string name;
        std::string description;
        json workflow_definition;
        json parameters;
        std::string type = "sequential"; // sequential, parallel, conditional
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct WorkflowResponse {
        bool success = false;
        std::string message;
        std::string workflow_id;
        std::string status;
        json result;
        std::vector<std::string> errors;
        
        json to_json() const;
    };

    struct WorkflowExecutionRequest {
        std::string workflow_id;
        json input_parameters;
        bool async_execution = true;
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct WorkflowExecutionResponse {
        bool success = false;
        std::string message;
        std::string execution_id;
        std::string status; // pending, running, completed, failed
        json output;
        std::vector<std::string> step_results;
        
        json to_json() const;
    };

    struct WorkflowStatusRequest {
        std::string workflow_id;
        std::string execution_id;
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct RAGWorkflowRequest {
        std::string query;
        std::string collection_name = "documents";
        int retrieve_k = 5;
        double score_threshold = 0.6;
        std::string completion_model;
        json rag_config;
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct RAGWorkflowResponse {
        bool success = false;
        std::string message;
        std::string response_text;
        std::vector<json> retrieved_documents;
        json metadata;
        
        json to_json() const;
    };

    // Session management
    struct SessionRequest {
        std::string session_name;
        json session_config;
        std::string workflow_type;
        
        void from_json(const json& j);
        bool validate() const;
    };

    struct SessionResponse {
        bool success = false;
        std::string message;
        std::string session_id;
        json session_info;
        
        json to_json() const;
    };

    // Core service methods
    std::future<WorkflowResponse> createWorkflow(const WorkflowRequest& request);
    std::future<WorkflowExecutionResponse> executeWorkflow(const WorkflowExecutionRequest& request);
    std::future<WorkflowResponse> getWorkflowStatus(const WorkflowStatusRequest& request);
    std::future<json> listWorkflows();
    std::future<WorkflowResponse> deleteWorkflow(const std::string& workflow_id);
    
    // RAG workflow operations
    std::future<RAGWorkflowResponse> executeRAGWorkflow(const RAGWorkflowRequest& request);
    std::future<RAGWorkflowResponse> searchRAGContext(const RAGWorkflowRequest& request);
    
    // Session management
    std::future<SessionResponse> createSession(const SessionRequest& request);
    std::future<SessionResponse> getSession(const std::string& session_id);
    std::future<json> listSessions();
    std::future<SessionResponse> deleteSession(const std::string& session_id);
    std::future<json> getSessionHistory(const std::string& session_id);

    // Orchestration operations
    std::future<json> createOrchestrationPlan(const json& request);
    std::future<json> executeOrchestrationPlan(const std::string& plan_id, const json& parameters);
    std::future<json> getOrchestrationStatus(const std::string& plan_id);

private:
    std::string generateWorkflowId();
    std::string generateExecutionId();
    std::string generateSessionId();
    std::string generateErrorMessage(const std::string& operation, const std::exception& e);
    
    // Internal workflow storage (in a real implementation, this would be persistent)
    std::map<std::string, json> workflows_;
    std::map<std::string, json> executions_;
    std::map<std::string, json> sessions_;
};

} // namespace kolosal::agents

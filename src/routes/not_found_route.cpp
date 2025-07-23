#include "kolosal/routes/not_found_route.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>

using json = nlohmann::json;

namespace kolosal::routes {

NotFoundRoute::NotFoundRoute() {}

bool NotFoundRoute::match(const std::string& method, const std::string& path) {
    // This route should match everything, but it should be added last
    // so it acts as a catch-all
    return true;
}

void NotFoundRoute::handle(SocketType sock, const std::string& body) {
    ServerLogger::logWarning("404 Not Found: Request to unimplemented endpoint");
    
    // Create helpful 404 response with available endpoints
    json response = {
        {"error", {
            {"message", "The requested endpoint was not found."},
            {"type", "invalid_request_error"},
            {"code", 404}
        }},
        {"available_endpoints", {
            {"health", "GET /health - Server health status"},
            {"models", "GET /v1/models - List available models"},
            {"chat_completions", "POST /v1/chat/completions - Chat completions"},
            {"completions", "POST /v1/completions - Text completions"},
            {"embeddings", "POST /v1/embeddings - Generate embeddings"},
            {"agents", {
                {"list", "GET /v1/agents - List all agents"},
                {"create", "POST /v1/agents - Create a new agent"},
                {"get", "GET /v1/agents/{id} - Get agent details"},
                {"delete", "DELETE /v1/agents/{id} - Delete an agent"},
                {"execute", "POST /v1/agents/{id}/execute - Execute agent task"},
                {"chat", "POST /v1/agents/{id}/chat - Chat with agent"},
                {"system_status", "GET /v1/agents/system/status - Agent system status"}
            }},
            {"documents", {
                {"add", "POST /add_documents - Add documents to knowledge base"},
                {"retrieve", "POST /retrieve - Retrieve relevant documents"},
                {"parse_pdf", "POST /parse_pdf - Parse PDF content"},
                {"parse_docx", "POST /parse_docx - Parse DOCX content"}
            }},
            {"rag", {
                {"chat", "POST /v1/rag/chat - RAG-enhanced chat"},
                {"search", "POST /v1/rag/search - Search knowledge base"}
            }},
            {"workflows", {
                {"create", "POST /v1/workflows - Create workflow"},
                {"execute", "POST /v1/workflows/{id}/execute - Execute workflow"},
                {"status", "GET /v1/workflows/{id}/status - Get workflow status"}
            }}
        }},
        {"documentation", "Visit the /docs endpoint for full API documentation"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"note", "This is a catch-all route. If you see this, the requested endpoint may not be implemented yet."}
    };
    
    // Add CORS headers
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, X-API-Key"}
    };
    
    send_response(sock, 404, response.dump(2), headers);
}

} // namespace kolosal::routes

#include "kolosal/routes/retrieve_test_route.hpp"
#include "kolosal/retrieval/retrieve_types.hpp"
#include "kolosal/retrieval/document_service.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <memory>

using json = nlohmann::json;

namespace kolosal
{

RetrieveTestRoute::RetrieveTestRoute()
{
    ServerLogger::logInfo("RetrieveTestRoute initialized");
}

RetrieveTestRoute::~RetrieveTestRoute() = default;

bool RetrieveTestRoute::match(const std::string& method, const std::string& path)
{
    return method == "GET" && path == "/retrieve/test";
}

void RetrieveTestRoute::handle(SocketType sock, const std::string& body)
{
    try
    {
        ServerLogger::logInfo("[Thread %u] Received retrieve test request", std::this_thread::get_id());

        json response;
        response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        response["test_name"] = "Document Retrieval Diagnostic";
        
        json tests = json::array();
        bool overall_success = true;

        // Test 1: Document Service Availability
        json test1;
        test1["name"] = "Document Service Availability";
        try 
        {
            auto& serverAPI = ServerAPI::instance();
            auto& document_service = serverAPI.getDocumentService();
            test1["status"] = "pass";
            test1["message"] = "Document service is available";
        }
        catch (const std::exception& ex)
        {
            test1["status"] = "fail";
            test1["message"] = "Document service not available: " + std::string(ex.what());
            overall_success = false;
        }
        tests.push_back(test1);

        // Test 2: Database Connection
        json test2;
        test2["name"] = "Database Connection";
        try 
        {
            auto& serverAPI = ServerAPI::instance();
            auto& document_service = serverAPI.getDocumentService();
            bool connected = document_service.testConnection().get();
            if (connected)
            {
                test2["status"] = "pass";
                test2["message"] = "Database connection successful";
            }
            else
            {
                test2["status"] = "fail";
                test2["message"] = "Database connection failed - check if Qdrant is running";
                overall_success = false;
            }
        }
        catch (const std::exception& ex)
        {
            test2["status"] = "fail";
            test2["message"] = "Database connection test error: " + std::string(ex.what());
            overall_success = false;
        }
        tests.push_back(test2);

        // Test 3: Embedding Generation
        json test3;
        test3["name"] = "Embedding Generation";
        try 
        {
            auto& serverAPI = ServerAPI::instance();
            auto& document_service = serverAPI.getDocumentService();
            
            // Test with a simple query
            std::string test_text = "test query for embedding";
            auto embedding_future = document_service.getEmbedding(test_text, "");
            auto embedding = embedding_future.get();
            
            if (!embedding.empty())
            {
                test3["status"] = "pass";
                test3["message"] = "Embedding generation successful (dimensions: " + std::to_string(embedding.size()) + ")";
                test3["embedding_dimensions"] = embedding.size();
            }
            else
            {
                test3["status"] = "fail";
                test3["message"] = "Embedding generation returned empty result";
                overall_success = false;
            }
        }
        catch (const std::exception& ex)
        {
            test3["status"] = "fail";
            test3["message"] = "Embedding generation failed: " + std::string(ex.what());
            overall_success = false;
        }
        tests.push_back(test3);

        // Test 4: Simple Retrieval Test
        json test4;
        test4["name"] = "Simple Retrieval Test";
        try 
        {
            auto& serverAPI = ServerAPI::instance();
            auto& document_service = serverAPI.getDocumentService();
            
            // Create a simple retrieval request
            kolosal::retrieval::RetrieveRequest request;
            request.query = "test query";
            request.k = 5;
            request.score_threshold = 0.0f;
            
            auto response_future = document_service.retrieveDocuments(request);
            auto retrieve_response = response_future.get();
            
            test4["status"] = "pass";
            test4["message"] = "Retrieval completed successfully (found " + std::to_string(retrieve_response.total_found) + " documents)";
            test4["documents_found"] = retrieve_response.total_found;
            
            if (retrieve_response.total_found == 0)
            {
                test4["note"] = "No documents found - this is normal if no documents have been indexed yet";
            }
        }
        catch (const std::exception& ex)
        {
            test4["status"] = "fail";
            test4["message"] = "Retrieval test failed: " + std::string(ex.what());
            overall_success = false;
        }
        tests.push_back(test4);

        // Add service health information
        try 
        {
            auto& serverAPI = ServerAPI::instance();
            auto& document_service = serverAPI.getDocumentService();
            auto healthStatus = document_service.getHealthStatus().get();
            response["service_health"] = healthStatus;
        }
        catch (const std::exception& ex)
        {
            response["service_health"] = {
                {"status", "unavailable"},
                {"error", ex.what()}
            };
        }

        response["tests"] = tests;
        response["overall_status"] = overall_success ? "pass" : "fail";
        
        // Add recommendations based on test results
        json recommendations = json::array();
        for (const auto& test : tests)
        {
            if (test["status"] == "fail")
            {
                std::string test_name = test["name"];
                std::string message = test["message"];
                
                if (test_name == "Document Service Availability")
                {
                    recommendations.push_back("Ensure the DocumentService is properly initialized in server configuration");
                }
                else if (test_name == "Database Connection")
                {
                    recommendations.push_back("Start Qdrant database server and verify connection settings");
                }
                else if (test_name == "Embedding Generation")
                {
                    recommendations.push_back("Check if embedding model is available and properly configured");
                }
                else if (test_name == "Simple Retrieval Test")
                {
                    recommendations.push_back("Check previous test failures or verify collection exists");
                }
            }
        }
        
        if (!recommendations.empty())
        {
            response["recommendations"] = recommendations;
        }

        // Send successful response
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        send_response(sock, 200, response.dump(2), headers);

        ServerLogger::logInfo("[Thread %u] Completed retrieve test - overall status: %s", 
                             std::this_thread::get_id(), overall_success ? "pass" : "fail");
    }
    catch (const std::exception& ex)
    {
        ServerLogger::logError("[Thread %u] Error handling retrieve test request: %s", 
                              std::this_thread::get_id(), ex.what());
        sendErrorResponse(sock, 500, "Internal server error: " + std::string(ex.what()), "server_error");
    }
}

void RetrieveTestRoute::sendErrorResponse(SocketType sock, int status, const std::string& message,
                                         const std::string& error_type, const std::string& param)
{
    json errorResponse;
    json error_obj;
    error_obj["message"] = message;
    error_obj["type"] = error_type;
    
    if (!param.empty())
    {
        error_obj["param"] = param;
    }
    
    errorResponse["error"] = error_obj;

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
    };
    send_response(sock, status, errorResponse.dump(), headers);
}

} // namespace kolosal

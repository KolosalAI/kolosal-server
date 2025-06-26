#include "kolosal/qdrant_client.hpp"
#include "kolosal/logger.hpp"
#include <curl/curl.h>
#include <thread>
#include <sstream>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <memory>
#include <random>
#include <map>
#include <atomic>

namespace kolosal
{

// Callback for libcurl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// HTTP request structure
struct HttpRequest
{
    std::string method;
    std::string url;
    std::string body;
    std::map<std::string, std::string> headers;
    int timeout = 30;
    
    // Response data
    std::string response_body;
    long response_code = 0;
    std::string error_message;
    
    // Promise for async operations
    std::shared_ptr<std::promise<QdrantResult>> promise;
};

// QdrantPoint implementations
nlohmann::json QdrantPoint::to_json() const
{
    nlohmann::json j;
    j["id"] = id;
    j["vector"] = vector;
    j["payload"] = payload;
    return j;
}

class QdrantClient::Impl
{
public:
    Config config_;
    CURLM* multi_handle_;
    std::queue<std::shared_ptr<HttpRequest>> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_thread_;
    std::atomic<bool> shutdown_{false};
    
    Impl(const Config& config) : config_(config)
    {
        // Initialize libcurl
        curl_global_init(CURL_GLOBAL_DEFAULT);
        multi_handle_ = curl_multi_init();
        
        // Start worker thread
        worker_thread_ = std::thread(&Impl::workerLoop, this);
        
        ServerLogger::logInfo("QdrantClient initialized - Host: %s:%d", 
                              config_.host.c_str(), config_.port);
    }
    
    ~Impl()
    {
        shutdown_ = true;
        queue_cv_.notify_all();
        
        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }
        
        if (multi_handle_)
        {
            curl_multi_cleanup(multi_handle_);
        }
        
        curl_global_cleanup();
    }
    
    std::string buildUrl(const std::string& endpoint) const
    {
        std::string protocol = config_.useHttps ? "https" : "http";
        return protocol + "://" + config_.host + ":" + std::to_string(config_.port) + endpoint;
    }
    
    std::future<QdrantResult> makeRequest(const std::string& method, 
                                         const std::string& endpoint,
                                         const std::string& body = "")
    {
        auto request = std::make_shared<HttpRequest>();
        request->method = method;
        request->url = buildUrl(endpoint);
        request->body = body;
        request->timeout = config_.timeout;
        request->promise = std::make_shared<std::promise<QdrantResult>>();
        
        // Set headers
        request->headers["Content-Type"] = "application/json";
        if (!config_.apiKey.empty())
        {
            request->headers["api-key"] = config_.apiKey;
        }
        
        auto future = request->promise->get_future();
        
        // Add to queue
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            request_queue_.push(request);
        }
        queue_cv_.notify_one();
        
        return future;
    }
    
    void workerLoop()
    {
        while (!shutdown_)
        {
            std::shared_ptr<HttpRequest> request;
            
            // Wait for request
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] { return !request_queue_.empty() || shutdown_; });
                
                if (shutdown_)
                    break;
                
                request = request_queue_.front();
                request_queue_.pop();
            }
            
            // Execute request
            executeRequest(request);
        }
    }
    
    void executeRequest(std::shared_ptr<HttpRequest> request)
    {
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            QdrantResult result;
            result.success = false;
            result.error_message = "Failed to initialize CURL";
            request->promise->set_value(result);
            return;
        }
        
        // Set basic options
        curl_easy_setopt(curl, CURLOPT_URL, request->url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &request->response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, request->timeout);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        // Set method and body
        if (request->method == "POST")
        {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!request->body.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body.c_str());
            }
        }
        else if (request->method == "PUT")
        {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!request->body.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body.c_str());
            }
        }
        else if (request->method == "DELETE")
        {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        
        // Set headers
        struct curl_slist* headers = nullptr;
        for (const auto& [key, value] : request->headers)
        {
            std::string header = key + ": " + value;
            headers = curl_slist_append(headers, header.c_str());
        }
        if (headers)
        {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
        
        // Perform request
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &request->response_code);
        
        // Parse result
        QdrantResult result = parseResponse(request, res);
        
        // Cleanup
        if (headers)
        {
            curl_slist_free_all(headers);
        }
        curl_easy_cleanup(curl);
        
        // Set promise result
        request->promise->set_value(result);
    }
    
    QdrantResult parseResponse(std::shared_ptr<HttpRequest> request, CURLcode curl_code)
    {
        QdrantResult result;
        result.success = false;
        result.status_code = static_cast<int>(request->response_code);
        
        if (curl_code != CURLE_OK)
        {
            result.error_message = "CURL error: " + std::string(curl_easy_strerror(curl_code));
            return result;
        }
        
        // Check HTTP status
        if (request->response_code >= 200 && request->response_code < 300)
        {
            result.success = true;
            
            // Try to parse JSON response for additional information
            try
            {
                auto json_response = nlohmann::json::parse(request->response_body);
                if (json_response.contains("result") && json_response["result"].contains("operation_id"))
                {
                    result.operation_id = json_response["result"]["operation_id"];
                }
            }
            catch (const std::exception&)
            {
                // Ignore JSON parsing errors for successful requests
            }
        }
        else
        {
            result.error_message = "HTTP error " + std::to_string(request->response_code);
            
            // Try to extract error details from response
            try
            {
                auto json_response = nlohmann::json::parse(request->response_body);
                if (json_response.contains("status") && json_response["status"].contains("error"))
                {
                    result.error_message += ": " + json_response["status"]["error"].get<std::string>();
                }
            }
            catch (const std::exception&)
            {
                result.error_message += ": " + request->response_body;
            }
        }
        
        return result;
    }
    
    std::string generateId()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::string id;
        for (int i = 0; i < 32; ++i)
        {
            if (i == 8 || i == 12 || i == 16 || i == 20)
            {
                id += "-";
            }
            id += "0123456789abcdef"[dis(gen)];
        }
        return id;
    }
};

// QdrantClient implementations
QdrantClient::QdrantClient(const Config& config)
    : pImpl(std::make_unique<Impl>(config))
{
}

QdrantClient::~QdrantClient() = default;

QdrantClient::QdrantClient(QdrantClient&&) noexcept = default;
QdrantClient& QdrantClient::operator=(QdrantClient&&) noexcept = default;

std::future<QdrantResult> QdrantClient::testConnection()
{
    return pImpl->makeRequest("GET", "/");
}

std::future<QdrantResult> QdrantClient::createCollection(
    const std::string& collection_name,
    int vector_size,
    const std::string& distance)
{
    nlohmann::json body;
    body["vectors"] = nlohmann::json::object();
    body["vectors"]["size"] = vector_size;
    body["vectors"]["distance"] = distance;
    
    return pImpl->makeRequest("PUT", "/collections/" + collection_name, body.dump());
}

std::future<QdrantResult> QdrantClient::collectionExists(const std::string& collection_name)
{
    return pImpl->makeRequest("GET", "/collections/" + collection_name);
}

std::future<QdrantResult> QdrantClient::upsertPoints(
    const std::string& collection_name,
    const std::vector<QdrantPoint>& points)
{
    nlohmann::json body;
    body["points"] = nlohmann::json::array();
    
    for (const auto& point : points)
    {
        body["points"].push_back(point.to_json());
    }
    
    return pImpl->makeRequest("PUT", "/collections/" + collection_name + "/points", body.dump());
}

std::future<QdrantResult> QdrantClient::deletePoints(
    const std::string& collection_name,
    const std::vector<std::string>& point_ids)
{
    nlohmann::json body;
    body["points"] = point_ids;
    
    return pImpl->makeRequest("POST", "/collections/" + collection_name + "/points/delete", body.dump());
}

std::future<QdrantResult> QdrantClient::search(
    const std::string& collection_name,
    const std::vector<float>& query_vector,
    int limit,
    float score_threshold)
{
    nlohmann::json body;
    body["vector"] = query_vector;
    body["limit"] = limit;
    body["score_threshold"] = score_threshold;
    body["with_payload"] = true;
    
    return pImpl->makeRequest("POST", "/collections/" + collection_name + "/points/search", body.dump());
}

} // namespace kolosal

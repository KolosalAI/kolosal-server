#include "kolosal/routes/internet_search_route.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <json.hpp>
#include <curl/curl.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>

using json = nlohmann::json;

namespace kolosal {

    // URL encoding function
    std::string urlEncode(const std::string& str) {
        std::ostringstream encoded;
        encoded.fill('0');
        encoded << std::hex;
        
        for (char c : str) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else {
                encoded << std::uppercase;
                encoded << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
                encoded << std::nouppercase;
            }
        }
        
        return encoded.str();
    }

    // Static callback for libcurl to write response data
    size_t InternetSearchRoute::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        size_t totalSize = size * nmemb;
        userp->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    InternetSearchRoute::InternetSearchRoute(const SearchConfig& config) : config_(config) {
        if (config_.enabled) {
            startWorkerThreads();
        }
    }

    InternetSearchRoute::~InternetSearchRoute() {
        stopWorkerThreads();
    }

    void InternetSearchRoute::startWorkerThreads() {
        // Start 2 worker threads for handling HTTP requests
        for (int i = 0; i < 2; ++i) {
            worker_threads_.emplace_back(&InternetSearchRoute::workerLoop, this);
        }
        ServerLogger::logInfo("Started %d worker threads for internet search", static_cast<int>(worker_threads_.size()));
    }

    void InternetSearchRoute::stopWorkerThreads() {
        shutdown_ = true;
        queue_cv_.notify_all();
        
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads_.clear();
        ServerLogger::logInfo("Stopped all internet search worker threads");
    }

    void InternetSearchRoute::workerLoop() {
        while (!shutdown_) {
            std::shared_ptr<HttpSearchRequest> request;
            
            // Wait for request
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] { return !request_queue_.empty() || shutdown_; });
                
                if (shutdown_) break;
                
                request = request_queue_.front();
                request_queue_.pop();
            }

            // Execute HTTP request
            SearchResult result;
            CURL* curl = curl_easy_init();
            if (!curl) {
                result.success = false;
                result.error_message = "Failed to initialize CURL";
                result.status_code = 0;
                request->promise->set_value(result);
                continue;
            }

            std::string response_body;
            
            try {
                // Set URL
                curl_easy_setopt(curl, CURLOPT_URL, request->url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
                
                // Set timeout
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, request->timeout);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
                
                // Set headers
                struct curl_slist* headers = nullptr;
                for (const auto& header : request->headers) {
                    std::string headerStr = header.first + ": " + header.second;
                    headers = curl_slist_append(headers, headerStr.c_str());
                }
                if (headers) {
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                }
                
                // Follow redirects
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
                
                // SSL options
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
                
                // Perform request
                CURLcode res = curl_easy_perform(curl);
                
                if (res == CURLE_OK) {
                    long response_code;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                    
                    result.success = (response_code >= 200 && response_code < 300);
                    result.response_body = response_body;
                    result.status_code = static_cast<int>(response_code);
                    
                    if (!result.success) {
                        result.error_message = "HTTP error " + std::to_string(response_code);
                    }
                } else {
                    result.success = false;
                    result.error_message = "CURL error: " + std::string(curl_easy_strerror(res));
                    result.status_code = 0;
                }
                
                if (headers) {
                    curl_slist_free_all(headers);
                }
            } catch (const std::exception& e) {
                result.success = false;
                result.error_message = "Exception during HTTP request: " + std::string(e.what());
                result.status_code = 0;
            }
            
            curl_easy_cleanup(curl);
            request->promise->set_value(result);
        }
    }

    std::future<SearchResult> InternetSearchRoute::makeHttpRequest(const std::string& url, 
                                                                   const std::map<std::string, std::string>& headers,
                                                                   int timeout) {
        auto request = std::make_shared<HttpSearchRequest>();
        request->url = url;
        request->headers = headers;
        request->timeout = timeout;
        request->promise = std::make_shared<std::promise<SearchResult>>();
        
        auto future = request->promise->get_future();
        
        // Add to queue
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            request_queue_.push(request);
        }
        queue_cv_.notify_one();
        
        return future;
    }

    bool InternetSearchRoute::match(const std::string& method, const std::string& path) {
        return (method == "GET" || method == "POST") && 
               (path == "/internet_search" || path == "/v1/internet_search" || path == "/search");
    }

    std::string InternetSearchRoute::buildSearchUrl(const SearchRequest& request) {
        std::string base_url = config_.searxng_url;
        if (base_url.back() == '/') {
            base_url.pop_back();
        }
        
        std::ostringstream url;
        url << base_url << "/search?q=" << urlEncode(request.query);
        
        if (!request.format.empty()) {
            url << "&format=" << urlEncode(request.format);
        } else {
            url << "&format=" << urlEncode(config_.default_format);
        }
        
        if (!request.language.empty()) {
            url << "&lang=" << urlEncode(request.language);
        } else if (!config_.default_language.empty()) {
            url << "&lang=" << urlEncode(config_.default_language);
        }
        
        if (!request.categories.empty()) {
            url << "&categories=" << urlEncode(request.categories);
        } else if (!config_.default_category.empty()) {
            url << "&categories=" << urlEncode(config_.default_category);
        }
        
        if (!request.engines.empty()) {
            url << "&engines=" << urlEncode(request.engines);
        } else if (!config_.default_engine.empty()) {
            url << "&engines=" << urlEncode(config_.default_engine);
        }
        
        if (request.results > 0) {
            url << "&pageno=1"; // SearXNG uses pageno for pagination
        }
        
        if (request.safe_search) {
            url << "&safesearch=1";
        } else {
            url << "&safesearch=0";
        }
        
        return url.str();
    }

    SearchRequest InternetSearchRoute::parseRequestBody(const std::string& body) {
        SearchRequest request;
        
        try {
            if (body.empty()) {
                return request; // Return empty request
            }
            
            json data = json::parse(body);
            
            if (data.contains("query") && data["query"].is_string()) {
                request.query = data["query"];
            }
            if (data.contains("q") && data["q"].is_string()) {
                request.query = data["q"]; // Alternative parameter name
            }
            if (data.contains("engines") && data["engines"].is_string()) {
                request.engines = data["engines"];
            }
            if (data.contains("categories") && data["categories"].is_string()) {
                request.categories = data["categories"];
            }
            if (data.contains("language") && data["language"].is_string()) {
                request.language = data["language"];
            }
            if (data.contains("lang") && data["lang"].is_string()) {
                request.language = data["lang"]; // Alternative parameter name
            }
            if (data.contains("format") && data["format"].is_string()) {
                request.format = data["format"];
            }
            if (data.contains("results") && data["results"].is_number_integer()) {
                request.results = data["results"];
            }
            if (data.contains("safe_search") && data["safe_search"].is_boolean()) {
                request.safe_search = data["safe_search"];
            }
            if (data.contains("safesearch") && data["safesearch"].is_boolean()) {
                request.safe_search = data["safesearch"]; // Alternative parameter name
            }
            if (data.contains("timeout") && data["timeout"].is_number_integer()) {
                request.timeout = data["timeout"];
            }
            
        } catch (const json::parse_error& e) {
            ServerLogger::logWarning("Failed to parse search request JSON: %s", e.what());
        }
        
        return request;
    }

    std::string InternetSearchRoute::validateRequest(const SearchRequest& request) {
        if (request.query.empty()) {
            return "Query parameter is required";
        }
        
        if (request.query.length() > 1000) {
            return "Query too long (max 1000 characters)";
        }
        
        if (request.results < 1 || request.results > 100) {
            return "Results parameter must be between 1 and 100";
        }
        
        if (request.timeout < 1 || request.timeout > 120) {
            return "Timeout must be between 1 and 120 seconds";
        }
        
        // Validate format
        if (!request.format.empty()) {
            if (request.format != "json" && request.format != "xml" && request.format != "csv" && request.format != "rss") {
                return "Invalid format. Supported formats: json, xml, csv, rss";
            }
        }
        
        return ""; // No errors
    }

    void InternetSearchRoute::handle(SocketType sock, const std::string& body) {
        try {
            if (!config_.enabled) {
                json error_response = {
                    {"error", {
                        {"type", "feature_disabled"},
                        {"message", "Internet search is not enabled on this server"}
                    }}
                };
                
                std::string response = "HTTP/1.1 503 Service Unavailable\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(static_cast<int>(error_response.dump().length())) + "\r\n";
                response += "Connection: close\r\n\r\n";
                response += error_response.dump();
                
                send(sock, response.c_str(), response.length(), 0);
                return;
            }
            
            ServerLogger::logInfo("[Thread %u] Received internet search request", 
                                   std::hash<std::thread::id>{}(std::this_thread::get_id()));
            
            // Parse request
            SearchRequest search_request = parseRequestBody(body);
            
            // Apply defaults
            if (search_request.results <= 0) {
                search_request.results = config_.max_results;
            }
            if (search_request.timeout <= 0) {
                search_request.timeout = config_.timeout;
            }
            if (search_request.format.empty()) {
                search_request.format = config_.default_format;
            }
            
            // Validate request
            std::string validation_error = validateRequest(search_request);
            if (!validation_error.empty()) {
                json error_response = {
                    {"error", {
                        {"type", "invalid_request"},
                        {"message", validation_error}
                    }}
                };
                
                std::string response = "HTTP/1.1 400 Bad Request\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(static_cast<int>(error_response.dump().length())) + "\r\n";
                response += "Connection: close\r\n\r\n";
                response += error_response.dump();
                
                send(sock, response.c_str(), response.length(), 0);
                return;
            }
            
            // Build search URL
            std::string search_url = buildSearchUrl(search_request);
            ServerLogger::logInfo("Making search request to: %s", search_url.c_str());
            
            // Prepare headers
            std::map<std::string, std::string> headers;
            headers["User-Agent"] = "KolosalServer/1.0";
            headers["Accept"] = "application/json, application/xml, text/csv, application/rss+xml";
            
            if (!config_.api_key.empty()) {
                headers["Authorization"] = "Bearer " + config_.api_key;
            }
            
            // Make HTTP request
            auto future = makeHttpRequest(search_url, headers, search_request.timeout);
            
            // Wait for result with timeout
            auto status = future.wait_for(std::chrono::seconds(search_request.timeout + 5));
            
            if (status == std::future_status::timeout) {
                json error_response = {
                    {"error", {
                        {"type", "timeout"},
                        {"message", "Search request timed out"}
                    }}
                };
                
                std::string response = "HTTP/1.1 504 Gateway Timeout\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(static_cast<int>(error_response.dump().length())) + "\r\n";
                response += "Connection: close\r\n\r\n";
                response += error_response.dump();
                
                send(sock, response.c_str(), response.length(), 0);
                return;
            }
            
            SearchResult result = future.get();
            
            if (!result.success) {
                json error_response = {
                    {"error", {
                        {"type", "search_failed"},
                        {"message", result.error_message},
                        {"status_code", result.status_code}
                    }}
                };
                
                std::string response = "HTTP/1.1 502 Bad Gateway\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(static_cast<int>(error_response.dump().length())) + "\r\n";
                response += "Connection: close\r\n\r\n";
                response += error_response.dump();
                
                send(sock, response.c_str(), response.length(), 0);
                return;
            }
            
            // Return the search results
            std::string content_type = "application/json";
            if (search_request.format == "xml") {
                content_type = "application/xml";
            } else if (search_request.format == "csv") {
                content_type = "text/csv";
            } else if (search_request.format == "rss") {
                content_type = "application/rss+xml";
            }
            
            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: " + content_type + "\r\n";
            response += "Content-Length: " + std::to_string(static_cast<int>(result.response_body.length())) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Connection: close\r\n\r\n";
            response += result.response_body;
            
            send(sock, response.c_str(), response.length(), 0);
            
            ServerLogger::logInfo("[Thread %u] Search request completed successfully", 
                                   std::hash<std::thread::id>{}(std::this_thread::get_id()));
            
        } catch (const std::exception& ex) {
            ServerLogger::logError("[Thread %u] Error handling search request: %s", 
                                   std::hash<std::thread::id>{}(std::this_thread::get_id()), ex.what());
            
            json error_response = {
                {"error", {
                    {"type", "internal_error"},
                    {"message", "Internal server error"}
                }}
            };
            
            std::string response = "HTTP/1.1 500 Internal Server Error\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(static_cast<int>(error_response.dump().length())) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += error_response.dump();
            
            send(sock, response.c_str(), response.length(), 0);
        }
    }

} // namespace kolosal

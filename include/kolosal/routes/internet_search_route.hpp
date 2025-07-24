#ifndef KOLOSAL_INTERNET_SEARCH_ROUTE_HPP
#define KOLOSAL_INTERNET_SEARCH_ROUTE_HPP

#include "route_interface.hpp"
#include "../server_config.hpp"
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace kolosal {

    struct SearchRequest {
        std::string query;
        std::string engines;
        std::string categories;
        std::string language;
        std::string format;
        int results = 20;
        bool safe_search = true;
        int timeout = 30;
        
        SearchRequest() = default;
    };

    struct SearchResult {
        bool success = false;
        std::string response_body;
        std::string error_message;
        int status_code = 0;
        
        SearchResult() = default;
    };

    struct HttpSearchRequest {
        std::string url;
        std::string method = "GET";
        std::map<std::string, std::string> headers;
        std::string body;
        int timeout = 30;
        std::shared_ptr<std::promise<SearchResult>> promise;
    };

    class InternetSearchRoute : public IRoute {
    private:
        SearchConfig config_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;
        std::queue<std::shared_ptr<HttpSearchRequest>> request_queue_;
        std::atomic<bool> shutdown_{false};
        std::vector<std::thread> worker_threads_;
        
        void startWorkerThreads();
        void stopWorkerThreads();
        void workerLoop();
        std::future<SearchResult> makeHttpRequest(const std::string& url, 
                                                  const std::map<std::string, std::string>& headers = {},
                                                  int timeout = 30);
        std::string buildSearchUrl(const SearchRequest& request);
        SearchRequest parseRequestBody(const std::string& body);
        std::string validateRequest(const SearchRequest& request);
        
        // Static callback for libcurl
        static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
        
    public:
        explicit InternetSearchRoute(const SearchConfig& config);
        ~InternetSearchRoute();
        
        bool match(const std::string& method, const std::string& path) override;
        void handle(SocketType sock, const std::string& body) override;
    };

} // namespace kolosal

#endif // KOLOSAL_INTERNET_SEARCH_ROUTE_HPP

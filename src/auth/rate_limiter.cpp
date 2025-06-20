#include "kolosal/auth/rate_limiter.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>

namespace kolosal
{    namespace auth
    {

        RateLimiter::RateLimiter()
            : config_(), lastGlobalCleanup_(std::chrono::steady_clock::now())
        {
            ServerLogger::logInfo("Rate limiter initialized with default config - Max requests: %zu, Window: %lld seconds, Enabled: %s",
                                  config_.maxRequests, config_.windowSize.count(), config_.enabled ? "true" : "false");
        }

        RateLimiter::RateLimiter(const Config &config)
            : config_(config), lastGlobalCleanup_(std::chrono::steady_clock::now())
        {
            ServerLogger::logInfo("Rate limiter initialized - Max requests: %zu, Window: %lld seconds, Enabled: %s",
                                  config_.maxRequests, config_.windowSize.count(), config_.enabled ? "true" : "false");
        }

        RateLimiter::RateLimitResult RateLimiter::checkRateLimit(const std::string &clientIP)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // If rate limiting is disabled, allow all requests
            if (!config_.enabled)
            {
                return RateLimitResult{true, 0, config_.maxRequests, config_.windowSize};
            }

            // Perform periodic cleanup
            performPeriodicCleanup();

            // Get or create client data
            auto &clientData = clients_[clientIP];

            // Clean up old requests for this client
            cleanupOldRequests(clientData);

            auto now = std::chrono::steady_clock::now();
            size_t currentRequests = clientData.requests.size();

            // Check if we've exceeded the rate limit
            if (currentRequests >= config_.maxRequests)
            {
                // Calculate when the window will reset (when the oldest request expires)
                auto oldestRequest = clientData.requests.front();
                auto resetTime = std::chrono::duration_cast<std::chrono::seconds>(
                    (oldestRequest + config_.windowSize) - now);

                ServerLogger::logWarning("Rate limit exceeded for client %s - Requests: %zu/%zu",
                                         clientIP.c_str(), currentRequests, config_.maxRequests);

                return RateLimitResult{false, currentRequests, 0, resetTime};
            }

            // Allow the request and record it
            clientData.requests.push_back(now);
            size_t remaining = config_.maxRequests - (currentRequests + 1);

            // Calculate reset time (end of current window)
            auto windowStart = now - config_.windowSize;
            auto resetTime = config_.windowSize;
            if (!clientData.requests.empty())
            {
                auto oldestInWindow = clientData.requests.front();
                if (oldestInWindow > windowStart)
                {
                    resetTime = std::chrono::duration_cast<std::chrono::seconds>(
                        (oldestInWindow + config_.windowSize) - now);
                }
            }

            ServerLogger::logDebug("Rate limit check passed for client %s - Requests: %zu/%zu, Remaining: %zu",
                                   clientIP.c_str(), currentRequests + 1, config_.maxRequests, remaining);

            return RateLimitResult{true, currentRequests + 1, remaining, resetTime};
        }

        void RateLimiter::updateConfig(const Config &config)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            config_ = config;
            ServerLogger::logInfo("Rate limiter configuration updated - Max requests: %zu, Window: %lld seconds, Enabled: %s",
                                  config_.maxRequests, config_.windowSize.count(), config_.enabled ? "true" : "false");
        }

        RateLimiter::Config RateLimiter::getConfig() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return config_;
        }

        void RateLimiter::clearClient(const std::string &clientIP)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = clients_.find(clientIP);
            if (it != clients_.end())
            {
                clients_.erase(it);
                ServerLogger::logInfo("Cleared rate limit data for client %s", clientIP.c_str());
            }
        }

        void RateLimiter::clearAll()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            clients_.clear();
            ServerLogger::logInfo("Cleared all rate limit data");
        }

        std::unordered_map<std::string, size_t> RateLimiter::getStatistics() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::unordered_map<std::string, size_t> stats;

            for (const auto &client : clients_)
            {
                stats[client.first] = client.second.requests.size();
            }

            return stats;
        }

        void RateLimiter::cleanupOldRequests(ClientData &data)
        {
            auto now = std::chrono::steady_clock::now();
            auto cutoff = now - config_.windowSize;

            // Remove requests older than the window
            while (!data.requests.empty() && data.requests.front() < cutoff)
            {
                data.requests.pop_front();
            }

            data.lastCleanup = now;
        }

        void RateLimiter::performPeriodicCleanup()
        {
            auto now = std::chrono::steady_clock::now();

            // Only perform global cleanup if enough time has passed
            if (now - lastGlobalCleanup_ < GLOBAL_CLEANUP_INTERVAL)
            {
                return;
            }

            // Remove clients that haven't made requests recently
            auto cutoff = now - (config_.windowSize + GLOBAL_CLEANUP_INTERVAL);
            auto it = clients_.begin();

            while (it != clients_.end())
            {
                if (it->second.lastCleanup < cutoff || it->second.requests.empty())
                {
                    ServerLogger::logDebug("Removing inactive client from rate limiter: %s", it->first.c_str());
                    it = clients_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            lastGlobalCleanup_ = now;
            ServerLogger::logDebug("Performed rate limiter cleanup - Active clients: %zu", clients_.size());
        }

    } // namespace auth
} // namespace kolosal

#pragma once

#include "../export.hpp"
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <deque>

namespace kolosal
{
    namespace auth
    {

        /**
         * @brief Rate limiter implementation using sliding window algorithm
         *
         * This class provides rate limiting functionality by tracking request timestamps
         * for each client IP address using a sliding window approach.
         */
        class KOLOSAL_SERVER_API RateLimiter
        {
        public:
            /**
             * @brief Configuration for rate limiting
             */
            struct Config
            {
                size_t maxRequests = 100;            // Maximum requests allowed
                std::chrono::seconds windowSize{60}; // Time window in seconds
                bool enabled = true;                 // Whether rate limiting is enabled

                Config() = default;
                Config(size_t max_req, std::chrono::seconds window)
                    : maxRequests(max_req), windowSize(window), enabled(true) {}
            };

            /**
             * @brief Result of rate limit check
             */
            struct RateLimitResult
            {
                bool allowed = true;               // Whether request is allowed
                size_t requestsUsed = 0;           // Number of requests used in current window
                size_t requestsRemaining = 0;      // Number of requests remaining
                std::chrono::seconds resetTime{0}; // Time until window resets

                RateLimitResult() = default;
                RateLimitResult(bool allow, size_t used, size_t remaining, std::chrono::seconds reset)
                    : allowed(allow), requestsUsed(used), requestsRemaining(remaining), resetTime(reset) {}
            };

        private:
            /**
             * @brief Request tracking data for a client
             */
            struct ClientData
            {
                std::deque<std::chrono::steady_clock::time_point> requests;
                std::chrono::steady_clock::time_point lastCleanup;

                ClientData() : lastCleanup(std::chrono::steady_clock::now()) {}
            };        public:
            /**
             * @brief Default constructor with default configuration
             */
            RateLimiter();

            /**
             * @brief Constructor with configuration
             * @param config Rate limiting configuration
             */
            explicit RateLimiter(const Config &config);

            /**
             * @brief Check if a request from the given client IP is allowed
             * @param clientIP The client's IP address
             * @return RateLimitResult containing the decision and rate limit information
             */
            RateLimitResult checkRateLimit(const std::string &clientIP);

            /**
             * @brief Update the rate limiter configuration
             * @param config New configuration
             */
            void updateConfig(const Config &config);

            /**
             * @brief Get current configuration
             * @return Current rate limiter configuration
             */
            Config getConfig() const;

            /**
             * @brief Clear all rate limit data for a specific client
             * @param clientIP The client's IP address
             */
            void clearClient(const std::string &clientIP);

            /**
             * @brief Clear all rate limit data
             */
            void clearAll();

            /**
             * @brief Get current statistics
             * @return Map of client IP to number of requests in current window
             */
            std::unordered_map<std::string, size_t> getStatistics() const;

        private:
            /**
             * @brief Clean up old requests outside the current window
             * @param data Client data to clean up
             */
            void cleanupOldRequests(ClientData &data);

            /**
             * @brief Periodic cleanup to prevent memory leaks
             */
            void performPeriodicCleanup();

            mutable std::mutex mutex_;
            Config config_;
            std::unordered_map<std::string, ClientData> clients_;
            std::chrono::steady_clock::time_point lastGlobalCleanup_;

            // Global cleanup interval (clean up inactive clients every hour)
            static constexpr std::chrono::minutes GLOBAL_CLEANUP_INTERVAL{60};
        };

    } // namespace auth
} // namespace kolosal

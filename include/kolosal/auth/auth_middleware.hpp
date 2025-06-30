#pragma once

#include "../export.hpp"
#include "rate_limiter.hpp"
#include "cors_handler.hpp"
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
using SocketType = SOCKET;
#else
#include <sys/socket.h>
#include <unistd.h>
using SocketType = int;
#endif

namespace kolosal
{
    namespace auth
    {

        /**
         * @brief Authentication middleware that combines rate limiting, CORS, and API key authentication
         *
         * This class provides a unified interface for handling authentication concerns
         * including API key validation, rate limiting and CORS validation.
         */
        class KOLOSAL_SERVER_API AuthMiddleware
        {
        public:
            /**
             * @brief API key authentication configuration
             */
            struct ApiKeyConfig
            {
                bool enabled = false;                          // Whether API key authentication is enabled
                bool required = false;                         // Whether API key is required for all requests
                std::string headerName = "X-API-Key";         // Header name for API key
                std::unordered_set<std::string> validKeys;    // Set of valid API keys

                ApiKeyConfig() = default;
            };

            /**
             * @brief Request information for authentication processing
             */
            struct RequestInfo
            {
                std::string method;                         // HTTP method
                std::string path;                           // Request path
                std::string clientIP;                       // Client IP address
                std::string origin;                         // Origin header
                std::string userAgent;                      // User-Agent header
                std::map<std::string, std::string> headers; // All request headers

                RequestInfo() = default;
                RequestInfo(const std::string &m, const std::string &p, const std::string &ip)
                    : method(m), path(p), clientIP(ip) {}
            };

            /**
             * @brief Result of authentication processing
             */
            struct AuthResult
            {
                bool allowed = true;                        // Whether request is allowed
                bool isPreflight = false;                   // Whether this is a CORS preflight
                int statusCode = 200;                       // HTTP status code
                std::string reason;                         // Reason for denial (if any)
                std::map<std::string, std::string> headers; // Headers to add to response

                // Rate limiting info
                size_t rateLimitUsed = 0;
                size_t rateLimitRemaining = 0;
                std::chrono::seconds rateLimitReset{0};

                AuthResult() = default;
                AuthResult(bool allow, const std::string &r = "")
                    : allowed(allow), reason(r) {}
            };        public:
            /**
             * @brief Default constructor with default configurations
             */
            AuthMiddleware();

            /**
             * @brief Constructor with rate limiter configuration
             * @param rateLimiterConfig Rate limiter configuration
             */
            explicit AuthMiddleware(const RateLimiter::Config &rateLimiterConfig);

            /**
             * @brief Constructor with all configurations
             * @param rateLimiterConfig Rate limiter configuration
             * @param corsConfig CORS configuration
             * @param apiKeyConfig API key configuration
             */
            AuthMiddleware(const RateLimiter::Config &rateLimiterConfig,
                          const CorsHandler::Config &corsConfig,
                          const ApiKeyConfig &apiKeyConfig);

            /**
             * @brief Process authentication for a request
             * @param requestInfo Information about the incoming request
             * @return AuthResult containing the decision and response headers
             */
            AuthResult processRequest(const RequestInfo &requestInfo);

            /**
             * @brief Update rate limiter configuration
             * @param config New rate limiter configuration
             */
            void updateRateLimiterConfig(const RateLimiter::Config &config);

            /**
             * @brief Update CORS configuration
             * @param config New CORS configuration
             */
            void updateCorsConfig(const CorsHandler::Config &config);

            /**
             * @brief Update API key configuration
             * @param config New API key configuration
             */
            void updateApiKeyConfig(const ApiKeyConfig &config);

            /**
             * @brief Get current rate limiter configuration
             * @return Rate limiter configuration
             */
            RateLimiter::Config getRateLimiterConfig() const;

            /**
             * @brief Get current CORS configuration
             * @return CORS configuration
             */
            CorsHandler::Config getCorsConfig() const;

            /**
             * @brief Get current API key configuration
             * @return API key configuration
             */
            ApiKeyConfig getApiKeyConfig() const;

            /**
             * @brief Get rate limiting statistics
             * @return Map of client IP to request count
             */
            std::unordered_map<std::string, size_t> getRateLimitStatistics() const;

            /**
             * @brief Clear rate limit data for a specific client
             * @param clientIP Client IP address
             */
            void clearRateLimitData(const std::string &clientIP);

            /**
             * @brief Clear all rate limit data
             */
            void clearAllRateLimitData();

            /**
             * @brief Check if an origin is allowed by CORS
             * @param origin Origin to check
             * @return True if allowed
             */
            bool isOriginAllowed(const std::string &origin) const;

            /**
             * @brief Add an allowed CORS origin
             * @param origin Origin to add
             */
            void addAllowedOrigin(const std::string &origin); /**
                                                               * @brief Remove an allowed CORS origin
                                                               * @param origin Origin to remove
                                                               */
            void removeAllowedOrigin(const std::string &origin);

            /**
             * @brief Get direct access to the rate limiter
             * @return Reference to the rate limiter
             */
            RateLimiter &getRateLimiter();
            const RateLimiter &getRateLimiter() const;

            /**
             * @brief Get direct access to the CORS handler
             * @return Reference to the CORS handler
             */
            CorsHandler &getCorsHandler();
            const CorsHandler &getCorsHandler() const;

            /**
             * @brief Validate an API key
             * @param apiKey API key to validate
             * @return True if the API key is valid
             */
            bool validateApiKey(const std::string& apiKey) const;

            /**
             * @brief Add a valid API key
             * @param apiKey API key to add
             */
            void addApiKey(const std::string& apiKey);

            /**
             * @brief Remove an API key
             * @param apiKey API key to remove
             */
            void removeApiKey(const std::string& apiKey);

            /**
             * @brief Clear all API keys
             */
            void clearApiKeys();

        private:
            /**
             * @brief Extract header value from request headers
             * @param headers Request headers
             * @param name Header name (case-insensitive)
             * @return Header value or empty string if not found
             */
            std::string getHeaderValue(const std::map<std::string, std::string> &headers,
                                       const std::string &name) const;

            /**
             * @brief Convert header name to lowercase for case-insensitive comparison
             * @param name Header name
             * @return Lowercase header name
             */
            inline std::string toLowercase(const std::string &name) const;

            /**
             * @brief Validate API key authentication
             * @param requestInfo Request information
             * @return True if API key authentication passes
             */
            bool validateApiKeyAuth(const RequestInfo& requestInfo) const;

            /**
             * @brief Simple cache for header lookups to improve performance
             */
            struct HeaderCache {
                std::unordered_map<std::string, std::string> cache;
                std::chrono::steady_clock::time_point lastClear;
                static constexpr size_t MAX_CACHE_SIZE = 1000;
                static constexpr auto CACHE_TTL = std::chrono::minutes(5);
                
                void clear() {
                    cache.clear();
                    lastClear = std::chrono::steady_clock::now();
                }
                
                bool shouldClear() const {
                    auto now = std::chrono::steady_clock::now();
                    return cache.size() > MAX_CACHE_SIZE || 
                           (now - lastClear) > CACHE_TTL;
                }
            };
            
            std::unique_ptr<RateLimiter> rateLimiter_;
            std::unique_ptr<CorsHandler> corsHandler_;
            ApiKeyConfig apiKeyConfig_;
            mutable HeaderCache headerCache_;
        };

    } // namespace auth
} // namespace kolosal

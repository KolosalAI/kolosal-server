#include "kolosal/auth/auth_middleware.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>

namespace kolosal
{
    namespace auth
    {

        AuthMiddleware::AuthMiddleware(const RateLimiter::Config &rateLimiterConfig,
                                       const CorsHandler::Config &corsConfig,
                                       const ApiKeyConfig &apiKeyConfig)
            : rateLimiter_(std::make_unique<RateLimiter>(rateLimiterConfig)),
              corsHandler_(std::make_unique<CorsHandler>(corsConfig)),
              apiKeyConfig_(apiKeyConfig)
        {

            ServerLogger::logInfo("Authentication middleware initialized with API key auth: %s",
                                  apiKeyConfig_.enabled ? "enabled" : "disabled");
        }

        AuthMiddleware::AuthResult AuthMiddleware::processRequest(const RequestInfo &requestInfo)
        {
            AuthResult result;
            ServerLogger::logInfo("Auth middleware processing request: %s %s from %s",
                                  requestInfo.method.c_str(), requestInfo.path.c_str(), requestInfo.clientIP.c_str());

            // Process CORS first
            std::string origin = getHeaderValue(requestInfo.headers, "Origin");
            std::string requestHeaders = getHeaderValue(requestInfo.headers, "Access-Control-Request-Headers");
            std::string requestMethod = getHeaderValue(requestInfo.headers, "Access-Control-Request-Method");
            ServerLogger::logInfo("CORS headers - Origin: %s, Request-Headers: %s, Request-Method: %s",
                                  origin.c_str(), requestHeaders.c_str(), requestMethod.c_str());
            auto corsResult = corsHandler_->processCors(requestInfo.method, origin, requestHeaders, requestMethod);
            ServerLogger::logInfo("CORS result - IsValid: %s, IsPreflight: %s",
                                  corsResult.isValid ? "true" : "false",
                                  corsResult.isPreflight ? "true" : "false");

            if (!corsResult.isValid)
            {
                result.allowed = false;
                result.statusCode = 403;
                result.reason = "CORS policy violation";
                ServerLogger::logWarning("CORS policy violation for request from %s to %s %s",
                                         requestInfo.clientIP.c_str(), requestInfo.method.c_str(), requestInfo.path.c_str());
                return result;
            }

            // Add CORS headers to response
            result.headers.insert(corsResult.headers.begin(), corsResult.headers.end());
            result.isPreflight = corsResult.isPreflight;
            // If this is a preflight request, we don't need to check rate limits or API keys
            if (corsResult.isPreflight)
            {
                result.statusCode = 204; // No Content for successful preflight
                ServerLogger::logDebug("CORS preflight request approved for %s", requestInfo.clientIP.c_str());
                return result;
            }

            // Check API key authentication if enabled
            if (apiKeyConfig_.enabled && !validateApiKeyAuth(requestInfo))
            {
                result.allowed = false;
                result.statusCode = 401; // Unauthorized
                result.reason = "Invalid or missing API key";
                ServerLogger::logWarning("API key authentication failed for request from %s to %s %s",
                                         requestInfo.clientIP.c_str(), requestInfo.method.c_str(), requestInfo.path.c_str());
                return result;
            }
            // Process rate limiting
            auto rateLimitResult = rateLimiter_->checkRateLimit(requestInfo.clientIP);
            ServerLogger::logInfo("Rate limit result - Allowed: %s, Used: %zu, Remaining: %zu",
                                  rateLimitResult.allowed ? "true" : "false",
                                  rateLimitResult.requestsUsed, rateLimitResult.requestsRemaining);

            if (!rateLimitResult.allowed)
            {
                result.allowed = false;
                result.statusCode = 429; // Too Many Requests
                result.reason = "Rate limit exceeded";

                // Add rate limit headers
                result.headers["X-Rate-Limit-Limit"] = std::to_string(rateLimiter_->getConfig().maxRequests);
                result.headers["X-Rate-Limit-Remaining"] = "0";
                result.headers["X-Rate-Limit-Reset"] = std::to_string(rateLimitResult.resetTime.count());
                result.headers["Retry-After"] = std::to_string(rateLimitResult.resetTime.count());

                ServerLogger::logWarning("Rate limit exceeded for client %s - %zu requests used",
                                         requestInfo.clientIP.c_str(), rateLimitResult.requestsUsed);
                return result;
            }

            // Add rate limit information to response headers
            result.headers["X-Rate-Limit-Limit"] = std::to_string(rateLimiter_->getConfig().maxRequests);
            result.headers["X-Rate-Limit-Remaining"] = std::to_string(rateLimitResult.requestsRemaining);
            result.headers["X-Rate-Limit-Reset"] = std::to_string(rateLimitResult.resetTime.count());

            // Store rate limit info in result
            result.rateLimitUsed = rateLimitResult.requestsUsed;
            result.rateLimitRemaining = rateLimitResult.requestsRemaining;
            result.rateLimitReset = rateLimitResult.resetTime;
            ServerLogger::logDebug("Request approved for client %s - Rate limit: %zu/%zu, CORS origin: %s",
                                   requestInfo.clientIP.c_str(),
                                   rateLimitResult.requestsUsed,
                                   rateLimiter_->getConfig().maxRequests,
                                   origin.empty() ? "none" : origin.c_str());

            ServerLogger::logInfo("Auth middleware completed - Request allowed: %s", result.allowed ? "true" : "false");

            return result;
        }

        void AuthMiddleware::updateRateLimiterConfig(const RateLimiter::Config &config)
        {
            rateLimiter_->updateConfig(config);
        }

        void AuthMiddleware::updateCorsConfig(const CorsHandler::Config &config)
        {
            corsHandler_->updateConfig(config);
        }

        void AuthMiddleware::updateApiKeyConfig(const ApiKeyConfig &config)
        {
            apiKeyConfig_ = config;
            ServerLogger::logInfo("API key configuration updated - Enabled: %s, Required: %s, Keys count: %zu",
                                  config.enabled ? "true" : "false",
                                  config.required ? "true" : "false",
                                  config.validKeys.size());
        }

        RateLimiter::Config AuthMiddleware::getRateLimiterConfig() const
        {
            return rateLimiter_->getConfig();
        }

        CorsHandler::Config AuthMiddleware::getCorsConfig() const
        {
            return corsHandler_->getConfig();
        }

        AuthMiddleware::ApiKeyConfig AuthMiddleware::getApiKeyConfig() const
        {
            return apiKeyConfig_;
        }

        std::unordered_map<std::string, size_t> AuthMiddleware::getRateLimitStatistics() const
        {
            return rateLimiter_->getStatistics();
        }

        void AuthMiddleware::clearRateLimitData(const std::string &clientIP)
        {
            rateLimiter_->clearClient(clientIP);
        }

        void AuthMiddleware::clearAllRateLimitData()
        {
            rateLimiter_->clearAll();
        }

        bool AuthMiddleware::isOriginAllowed(const std::string &origin) const
        {
            return corsHandler_->isOriginAllowed(origin);
        }

        void AuthMiddleware::addAllowedOrigin(const std::string &origin)
        {
            corsHandler_->addAllowedOrigin(origin);
        }

        void AuthMiddleware::removeAllowedOrigin(const std::string &origin)
        {
            corsHandler_->removeAllowedOrigin(origin);
        }

        RateLimiter &AuthMiddleware::getRateLimiter()
        {
            return *rateLimiter_;
        }

        const RateLimiter &AuthMiddleware::getRateLimiter() const
        {
            return *rateLimiter_;
        }

        CorsHandler &AuthMiddleware::getCorsHandler()
        {
            return *corsHandler_;
        }

        const CorsHandler &AuthMiddleware::getCorsHandler() const
        {
            return *corsHandler_;
        }

        bool AuthMiddleware::validateApiKey(const std::string &apiKey) const
        {
            return !apiKey.empty() && apiKeyConfig_.validKeys.find(apiKey) != apiKeyConfig_.validKeys.end();
        }

        void AuthMiddleware::addApiKey(const std::string &apiKey)
        {
            if (!apiKey.empty())
            {
                apiKeyConfig_.validKeys.insert(apiKey);
                ServerLogger::logInfo("API key added (total: %zu keys)", apiKeyConfig_.validKeys.size());
            }
        }

        void AuthMiddleware::removeApiKey(const std::string &apiKey)
        {
            auto it = apiKeyConfig_.validKeys.find(apiKey);
            if (it != apiKeyConfig_.validKeys.end())
            {
                apiKeyConfig_.validKeys.erase(it);
                ServerLogger::logInfo("API key removed (total: %zu keys)", apiKeyConfig_.validKeys.size());
            }
        }

        void AuthMiddleware::clearApiKeys()
        {
            apiKeyConfig_.validKeys.clear();
            ServerLogger::logInfo("All API keys cleared");
        }

        std::string AuthMiddleware::getHeaderValue(const std::map<std::string, std::string> &headers,
                                                   const std::string &name) const
        {
            std::string lowerName = toLowercase(name);

            for (const auto &header : headers)
            {
                if (toLowercase(header.first) == lowerName)
                {
                    return header.second;
                }
            }

            return "";
        }

        std::string AuthMiddleware::toLowercase(const std::string &name) const
        {
            std::string result = name;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return result;
        }

        bool AuthMiddleware::validateApiKeyAuth(const RequestInfo &requestInfo) const
        {
            // If API key authentication is not required, always pass
            if (!apiKeyConfig_.required)
            {
                return true;
            }

            // Get the API key from the configured header
            std::string apiKey = getHeaderValue(requestInfo.headers, apiKeyConfig_.headerName);

            // Handle Bearer token format if using Authorization header
            if (toLowercase(apiKeyConfig_.headerName) == "authorization" && !apiKey.empty())
            {
                if (apiKey.substr(0, 7) == "Bearer ")
                {
                    apiKey = apiKey.substr(7);
                }
            }

            // Validate the API key
            bool isValid = validateApiKey(apiKey);

            if (isValid)
            {
                ServerLogger::logDebug("API key authentication successful for %s %s from %s",
                                       requestInfo.method.c_str(), requestInfo.path.c_str(), requestInfo.clientIP.c_str());
            }
            else
            {
                ServerLogger::logWarning("API key authentication failed for %s %s from %s - Key: %s",
                                         requestInfo.method.c_str(), requestInfo.path.c_str(), requestInfo.clientIP.c_str(),
                                         apiKey.empty() ? "(missing)" : "(invalid)");
            }

            return isValid;
        }

    } // namespace auth
} // namespace kolosal

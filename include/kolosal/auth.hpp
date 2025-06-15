#pragma once

/**
 * @file auth.hpp
 * @brief Main authentication header that includes all authentication components
 * 
 * This header provides a convenient way to include all authentication features
 * including rate limiting, CORS handling, and the unified middleware.
 */

#include "auth/rate_limiter.hpp"
#include "auth/cors_handler.hpp"
#include "auth/auth_middleware.hpp"

namespace kolosal {
namespace auth {

/**
 * @brief Create a default authentication middleware with sensible defaults
 * @return Configured AuthMiddleware instance
 */
inline std::unique_ptr<AuthMiddleware> createDefaultAuthMiddleware() {
    RateLimiter::Config rateLimitConfig;
    rateLimitConfig.maxRequests = 100;           // 100 requests per minute
    rateLimitConfig.windowSize = std::chrono::seconds(60);
    rateLimitConfig.enabled = true;
    
    CorsHandler::Config corsConfig;
    corsConfig.allowedOrigins = {"*"};           // Allow all origins by default
    corsConfig.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD", "PATCH"};
    corsConfig.allowedHeaders = {"Content-Type", "Authorization", "X-Requested-With", "Accept", "Origin"};
    corsConfig.allowCredentials = false;
    corsConfig.maxAge = 86400;                   // 24 hours
    corsConfig.enabled = true;
    
    return std::make_unique<AuthMiddleware>(rateLimitConfig, corsConfig);
}

/**
 * @brief Create a production-ready authentication middleware with stricter settings
 * @return Configured AuthMiddleware instance for production use
 */
inline std::unique_ptr<AuthMiddleware> createProductionAuthMiddleware() {
    RateLimiter::Config rateLimitConfig;
    rateLimitConfig.maxRequests = 60;            // 60 requests per minute (1 per second average)
    rateLimitConfig.windowSize = std::chrono::seconds(60);
    rateLimitConfig.enabled = true;
    
    CorsHandler::Config corsConfig;
    corsConfig.allowedOrigins = {};              // Must be explicitly configured
    corsConfig.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
    corsConfig.allowedHeaders = {"Content-Type", "Authorization", "Accept"};
    corsConfig.allowCredentials = true;
    corsConfig.maxAge = 3600;                    // 1 hour
    corsConfig.enabled = true;
    
    return std::make_unique<AuthMiddleware>(rateLimitConfig, corsConfig);
}

/**
 * @brief Create a development authentication middleware with relaxed settings
 * @return Configured AuthMiddleware instance for development use
 */
inline std::unique_ptr<AuthMiddleware> createDevelopmentAuthMiddleware() {
    RateLimiter::Config rateLimitConfig;
    rateLimitConfig.maxRequests = 1000;          // Very high limit for development
    rateLimitConfig.windowSize = std::chrono::seconds(60);
    rateLimitConfig.enabled = false;             // Disabled by default for development
    
    CorsHandler::Config corsConfig;
    corsConfig.allowedOrigins = {"*"};           // Allow all origins
    corsConfig.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD", "PATCH"};
    corsConfig.allowedHeaders = {"*"};           // Allow all headers
    corsConfig.allowCredentials = true;
    corsConfig.maxAge = 86400;
    corsConfig.enabled = true;
    
    return std::make_unique<AuthMiddleware>(rateLimitConfig, corsConfig);
}

} // namespace auth
} // namespace kolosal

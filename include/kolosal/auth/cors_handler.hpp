#pragma once

#include "../export.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <map>

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
         * @brief CORS (Cross-Origin Resource Sharing) handler
         *
         * This class provides CORS functionality to handle cross-origin requests
         * with configurable policies for origins, methods, and headers.
         */
        class KOLOSAL_SERVER_API CorsHandler
        {
        public:
            /**
             * @brief CORS configuration
             */
            struct Config
            {
                std::vector<std::string> allowedOrigins; // Allowed origins (use "*" for all)
                std::vector<std::string> allowedMethods; // Allowed HTTP methods
                std::vector<std::string> allowedHeaders; // Allowed headers
                std::vector<std::string> exposedHeaders; // Headers exposed to the browser
                bool allowCredentials = false;           // Whether to allow credentials
                int maxAge = 86400;                      // Preflight cache duration in seconds
                bool enabled = true;                     // Whether CORS is enabled

                Config()
                {
                    // Default configuration
                    allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD", "PATCH"};
                    allowedHeaders = {"Content-Type", "Authorization", "X-Requested-With", "Accept", "Origin"};
                    exposedHeaders = {"X-Total-Count", "X-Rate-Limit-Limit", "X-Rate-Limit-Remaining", "X-Rate-Limit-Reset"};
                    allowedOrigins = {"*"}; // Allow all origins by default
                }
            };

            /**
             * @brief Result of CORS processing
             */
            struct CorsResult
            {
                bool isValid = true;                        // Whether the request is valid
                bool isPreflight = false;                   // Whether this is a preflight request
                std::map<std::string, std::string> headers; // Headers to add to response
                int statusCode = 200;                       // Status code for preflight responses

                CorsResult() = default;
                CorsResult(bool valid, bool preflight = false)
                    : isValid(valid), isPreflight(preflight) {}
            };        public:
            /**
             * @brief Default constructor with default configuration
             */
            CorsHandler();

            /**
             * @brief Constructor with configuration
             * @param config CORS configuration
             */
            explicit CorsHandler(const Config &config);

            /**
             * @brief Process CORS for an incoming request
             * @param method HTTP method
             * @param origin Origin header value
             * @param requestHeaders Requested headers (for preflight)
             * @param requestMethod Requested method (for preflight)
             * @return CorsResult containing headers and validation result
             */
            CorsResult processCors(const std::string &method,
                                   const std::string &origin,
                                   const std::string &requestHeaders = "",
                                   const std::string &requestMethod = "");

            /**
             * @brief Update CORS configuration
             * @param config New configuration
             */
            void updateConfig(const Config &config);

            /**
             * @brief Get current configuration
             * @return Current CORS configuration
             */
            Config getConfig() const;

            /**
             * @brief Add an allowed origin
             * @param origin Origin to allow
             */
            void addAllowedOrigin(const std::string &origin);

            /**
             * @brief Remove an allowed origin
             * @param origin Origin to remove
             */
            void removeAllowedOrigin(const std::string &origin);

            /**
             * @brief Check if an origin is allowed
             * @param origin Origin to check
             * @return True if the origin is allowed
             */
            bool isOriginAllowed(const std::string &origin) const;

        private:
            /**
             * @brief Check if a method is allowed
             * @param method HTTP method to check
             * @return True if the method is allowed
             */
            bool isMethodAllowed(const std::string &method) const;

            /**
             * @brief Check if headers are allowed
             * @param headers Comma-separated list of headers
             * @return True if all headers are allowed
             */
            bool areHeadersAllowed(const std::string &headers) const;

            /**
             * @brief Convert vector to comma-separated string
             * @param items Vector of strings
             * @return Comma-separated string
             */
            std::string vectorToString(const std::vector<std::string> &items) const;

            /**
             * @brief Parse comma-separated string to vector
             * @param str Comma-separated string
             * @return Vector of trimmed strings
             */
            std::vector<std::string> parseHeaderList(const std::string &str) const;

            /**
             * @brief Trim whitespace from string
             * @param str String to trim
             * @return Trimmed string
             */
            std::string trim(const std::string &str) const;

            Config config_;
            std::unordered_set<std::string> allowedOriginsSet_; // For faster lookup
            std::unordered_set<std::string> allowedMethodsSet_; // For faster lookup
            std::unordered_set<std::string> allowedHeadersSet_; // For faster lookup
        };

    } // namespace auth
} // namespace kolosal

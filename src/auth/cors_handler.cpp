#include "kolosal/auth/cors_handler.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>
#include <sstream>

namespace kolosal
{    namespace auth
    {

        CorsHandler::CorsHandler() : config_()
        {
            updateConfig(config_);
        }

        CorsHandler::CorsHandler(const Config &config) : config_(config)
        {
            updateConfig(config);
        }

        CorsHandler::CorsResult CorsHandler::processCors(const std::string &method,
                                                         const std::string &origin,
                                                         const std::string &requestHeaders,
                                                         const std::string &requestMethod)
        {
            if (!config_.enabled)
            {
                return CorsResult{true, false};
            }

            CorsResult result;

            // Check if origin is allowed
            if (!origin.empty() && !isOriginAllowed(origin))
            {
                ServerLogger::logWarning("CORS: Origin not allowed: %s", origin.c_str());
                result.isValid = false;
                return result;
            }

            // Set the Access-Control-Allow-Origin header
            if (!origin.empty())
            {
                if (config_.allowedOrigins.size() == 1 && config_.allowedOrigins[0] == "*")
                {
                    result.headers["Access-Control-Allow-Origin"] = "*";
                }
                else
                {
                    result.headers["Access-Control-Allow-Origin"] = origin;
                }
            }

            // Handle preflight requests (OPTIONS method)
            if (method == "OPTIONS" && !requestMethod.empty())
            {
                result.isPreflight = true;

                // Check if the requested method is allowed
                if (!isMethodAllowed(requestMethod))
                {
                    ServerLogger::logWarning("CORS: Method not allowed in preflight: %s", requestMethod.c_str());
                    result.isValid = false;
                    return result;
                }

                // Check if the requested headers are allowed
                if (!requestHeaders.empty() && !areHeadersAllowed(requestHeaders))
                {
                    ServerLogger::logWarning("CORS: Headers not allowed in preflight: %s", requestHeaders.c_str());
                    result.isValid = false;
                    return result;
                }

                // Set preflight response headers
                result.headers["Access-Control-Allow-Methods"] = vectorToString(config_.allowedMethods);
                result.headers["Access-Control-Allow-Headers"] = vectorToString(config_.allowedHeaders);
                result.headers["Access-Control-Max-Age"] = std::to_string(config_.maxAge);

                ServerLogger::logDebug("CORS: Preflight request approved for origin: %s, method: %s",
                                       origin.c_str(), requestMethod.c_str());
            }
            else
            {
                // Handle actual request
                if (!isMethodAllowed(method))
                {
                    ServerLogger::logWarning("CORS: Method not allowed: %s", method.c_str());
                    result.isValid = false;
                    return result;
                }

                ServerLogger::logDebug("CORS: Request approved for origin: %s, method: %s",
                                       origin.c_str(), method.c_str());
            }

            // Set credentials header if configured
            if (config_.allowCredentials)
            {
                result.headers["Access-Control-Allow-Credentials"] = "true";
            }

            // Set exposed headers
            if (!config_.exposedHeaders.empty())
            {
                result.headers["Access-Control-Expose-Headers"] = vectorToString(config_.exposedHeaders);
            }

            return result;
        }

        void CorsHandler::updateConfig(const Config &config)
        {
            config_ = config;

            // Update sets for faster lookup
            allowedOriginsSet_.clear();
            for (const auto &origin : config_.allowedOrigins)
            {
                allowedOriginsSet_.insert(origin);
            }

            allowedMethodsSet_.clear();
            for (const auto &method : config_.allowedMethods)
            {
                allowedMethodsSet_.insert(method);
            }

            allowedHeadersSet_.clear();
            for (const auto &header : config_.allowedHeaders)
            {
                allowedHeadersSet_.insert(header);
            }

            ServerLogger::logInfo("CORS configuration updated - Enabled: %s, Origins: %zu, Methods: %zu, Headers: %zu",
                                  config_.enabled ? "true" : "false",
                                  config_.allowedOrigins.size(),
                                  config_.allowedMethods.size(),
                                  config_.allowedHeaders.size());
        }

        CorsHandler::Config CorsHandler::getConfig() const
        {
            return config_;
        }

        void CorsHandler::addAllowedOrigin(const std::string &origin)
        {
            auto it = std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin);
            if (it == config_.allowedOrigins.end())
            {
                config_.allowedOrigins.push_back(origin);
                allowedOriginsSet_.insert(origin);
                ServerLogger::logInfo("CORS: Added allowed origin: %s", origin.c_str());
            }
        }

        void CorsHandler::removeAllowedOrigin(const std::string &origin)
        {
            auto it = std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin);
            if (it != config_.allowedOrigins.end())
            {
                config_.allowedOrigins.erase(it);
                allowedOriginsSet_.erase(origin);
                ServerLogger::logInfo("CORS: Removed allowed origin: %s", origin.c_str());
            }
        }

        bool CorsHandler::isOriginAllowed(const std::string &origin) const
        {
            if (allowedOriginsSet_.count("*") > 0)
            {
                return true;
            }
            return allowedOriginsSet_.count(origin) > 0;
        }

        bool CorsHandler::isMethodAllowed(const std::string &method) const
        {
            return allowedMethodsSet_.count(method) > 0;
        }

        bool CorsHandler::areHeadersAllowed(const std::string &headers) const
        {
            if (headers.empty())
            {
                return true;
            }

            auto headerList = parseHeaderList(headers);
            for (const auto &header : headerList)
            {
                std::string lowerHeader = header;
                std::transform(lowerHeader.begin(), lowerHeader.end(), lowerHeader.begin(), ::tolower);

                // Check if this specific header is allowed
                bool found = false;
                for (const auto &allowedHeader : config_.allowedHeaders)
                {
                    std::string lowerAllowed = allowedHeader;
                    std::transform(lowerAllowed.begin(), lowerAllowed.end(), lowerAllowed.begin(), ::tolower);
                    if (lowerHeader == lowerAllowed)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }
            }

            return true;
        }

        std::string CorsHandler::vectorToString(const std::vector<std::string> &items) const
        {
            if (items.empty())
            {
                return "";
            }

            std::ostringstream oss;
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (i > 0)
                {
                    oss << ", ";
                }
                oss << items[i];
            }
            return oss.str();
        }

        std::vector<std::string> CorsHandler::parseHeaderList(const std::string &str) const
        {
            std::vector<std::string> result;
            std::istringstream iss(str);
            std::string item;

            while (std::getline(iss, item, ','))
            {
                std::string trimmed = trim(item);
                if (!trimmed.empty())
                {
                    result.push_back(trimmed);
                }
            }

            return result;
        }

        std::string CorsHandler::trim(const std::string &str) const
        {
            size_t start = str.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
            {
                return "";
            }

            size_t end = str.find_last_not_of(" \t\r\n");
            return str.substr(start, end - start + 1);
        }

    } // namespace auth
} // namespace kolosal

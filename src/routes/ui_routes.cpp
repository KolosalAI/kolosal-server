#include "kolosal/routes/ui_routes.hpp"
#include "kolosal/utils.hpp"
#include "kolosal/logger.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <thread>
#include <algorithm>
#include <vector>

namespace kolosal {

    bool UIRoute::match(const std::string &method, const std::string &path) {
        if (method == "GET" || method == "OPTIONS") {
            // Extract the path without query parameters
            std::string cleanPath = path;
            size_t queryPos = path.find('?');
            if (queryPos != std::string::npos) {
                cleanPath = path.substr(0, queryPos);
            }
            
            current_method_ = method;
            
            // Match root/main playground route
            if (cleanPath == "/" || cleanPath == "/playground" || cleanPath == "/playground/") {
                current_path_ = "/index.html";
                return true;
            }
            
            // Match test integration page
            if (cleanPath == "/test" || cleanPath == "/test-integration" || cleanPath == "/test-integration.html") {
                current_path_ = "/test-integration.html";
                return true;
            }
            
            // Match discover routes
            if (cleanPath == "/discover" || cleanPath == "/discover/" || cleanPath == "/discover/index.html") {
                current_path_ = "/discover/index.html";
                return true;
            }
            
            // Match model routes
            if (cleanPath == "/model" || cleanPath == "/model/" || cleanPath == "/model/index.html") {
                current_path_ = "/model/index.html";
                return true;
            }
            
            // Match retrieve routes
            if (cleanPath == "/retrieve" || cleanPath == "/retrieve/" || cleanPath == "/retrieve/index.html") {
                current_path_ = "/retrieve/index.html";
                return true;
            }
            
            if (cleanPath == "/retrieve/collection" || cleanPath == "/retrieve/collection.html") {
                current_path_ = "/retrieve/collection.html";
                return true;
            }
            
            if (cleanPath == "/retrieve/search" || cleanPath == "/retrieve/search.html") {
                current_path_ = "/retrieve/search.html";
                return true;
            }
            
            if (cleanPath == "/retrieve/upload" || cleanPath == "/retrieve/upload.html") {
                current_path_ = "/retrieve/upload.html";
                return true;
            }
            
            // Check if it's a static asset (CSS, JS, or other files)
            if (isStaticAsset(cleanPath)) {
                current_path_ = cleanPath;
                return true;
            }
        }
        
        return false;
    }

    bool UIRoute::isStaticAsset(const std::string& path) {
        // Get file extension
        size_t lastDot = path.find_last_of('.');
        if (lastDot == std::string::npos) {
            return false;
        }
        
        std::string extension = path.substr(lastDot);
        
        // Check for supported asset extensions
        std::vector<std::string> assetExtensions = {
            ".css", ".scss", ".js", ".json", ".png", ".jpg", ".jpeg", 
            ".gif", ".svg", ".ico", ".woff", ".woff2", ".ttf", ".eot",
            ".map", ".txt", ".md"
        };
        
        for (const auto& ext : assetExtensions) {
            if (extension == ext) {
                return true;
            }
        }
        
        return false;
    }

    void UIRoute::handle(SocketType sock, const std::string &body) {
        try {
            // Handle OPTIONS request for CORS preflight
            if (current_method_ == "OPTIONS") {
                ServerLogger::logDebug("[Thread %u] Handling OPTIONS request for UI endpoint: %s", 
                                     std::this_thread::get_id(), current_path_.c_str());
                
                std::map<std::string, std::string> headers = {
                    {"Content-Type", "text/plain"},
                    {"Access-Control-Allow-Origin", "*"},
                    {"Access-Control-Allow-Methods", "GET, OPTIONS"},
                    {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
                    {"Access-Control-Max-Age", "86400"} // Cache preflight for 24 hours
                };
                
                send_response(sock, 200, "", headers);
                return;
            }

            ServerLogger::logDebug("[Thread %u] Serving UI file: %s", 
                                 std::this_thread::get_id(), current_path_.c_str());

            // Determine content type
            std::string contentType = getContentType(current_path_);
            
            // Serve the file
            serveStaticFile(sock, current_path_, contentType);
            
        } catch (const std::exception &ex) {
            ServerLogger::logError("[Thread %u] Error serving UI file %s: %s", 
                                 std::this_thread::get_id(), current_path_.c_str(), ex.what());
            serve404(sock);
        }
    }

    std::string UIRoute::getContentType(const std::string& filePath) {
        // Get file extension
        size_t lastDot = filePath.find_last_of('.');
        if (lastDot == std::string::npos) {
            return "text/plain; charset=utf-8";
        }
        
        std::string extension = filePath.substr(lastDot);
        
        // Convert to lowercase for case-insensitive comparison
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        if (extension == ".html") {
            return "text/html; charset=utf-8";
        } else if (extension == ".css") {
            return "text/css; charset=utf-8";
        } else if (extension == ".scss") {
            return "text/css; charset=utf-8";
        } else if (extension == ".js") {
            return "application/javascript; charset=utf-8";
        } else if (extension == ".json") {
            return "application/json; charset=utf-8";
        } else if (extension == ".map") {
            return "application/json; charset=utf-8";
        } else if (extension == ".png") {
            return "image/png";
        } else if (extension == ".jpg" || extension == ".jpeg") {
            return "image/jpeg";
        } else if (extension == ".gif") {
            return "image/gif";
        } else if (extension == ".svg") {
            return "image/svg+xml";
        } else if (extension == ".ico") {
            return "image/x-icon";
        } else if (extension == ".woff") {
            return "font/woff";
        } else if (extension == ".woff2") {
            return "font/woff2";
        } else if (extension == ".ttf") {
            return "font/ttf";
        } else if (extension == ".eot") {
            return "application/vnd.ms-fontobject";
        } else if (extension == ".txt") {
            return "text/plain; charset=utf-8";
        } else if (extension == ".md") {
            return "text/markdown; charset=utf-8";
        }
        
        return "text/plain; charset=utf-8";
    }

    std::string UIRoute::readStaticFile(const std::string& relativePath) {
        // Get the absolute path to the executable's directory
        std::filesystem::path executablePath = std::filesystem::current_path();
        
        // All files are served from the kolosal-product directory
        std::filesystem::path staticDir = executablePath / "static" / "kolosal-product";
        
        // Remove leading slash from path
        std::string filePath = relativePath;
        if (!filePath.empty() && filePath[0] == '/') {
            filePath = filePath.substr(1);
        }
        
        std::filesystem::path fullPath = staticDir / filePath;
        
        // Try to resolve the canonical path
        std::filesystem::path canonicalPath;
        std::filesystem::path canonicalStatic;
        
        try {
            // Check if the file exists first
            if (!std::filesystem::exists(fullPath)) {
                throw std::runtime_error("File not found: " + relativePath);
            }
            
            canonicalPath = std::filesystem::canonical(fullPath);
            canonicalStatic = std::filesystem::canonical(staticDir);
        } catch (const std::filesystem::filesystem_error& ex) {
            throw std::runtime_error("File access error: " + std::string(ex.what()));
        }
        
        // Security check: ensure the path is within the static directory
        std::string pathStr = canonicalPath.string();
        std::string staticStr = canonicalStatic.string();
        
        // Normalize path separators for comparison
        std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
        std::replace(staticStr.begin(), staticStr.end(), '\\', '/');
        
        if (pathStr.find(staticStr) != 0) {
            throw std::runtime_error("Path traversal attack detected: " + relativePath);
        }
        
        // Read the file
        std::ifstream file(canonicalPath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + relativePath);
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void UIRoute::serveStaticFile(SocketType sock, const std::string& filePath, const std::string& contentType) {
        try {
            std::string content = readStaticFile(filePath);
            
            // Determine cache duration based on file type
            std::string cacheControl;
            if (contentType.find("image/") == 0 || contentType.find("font/") == 0) {
                // Images and fonts - cache for 1 week
                cacheControl = "public, max-age=604800";
            } else if (contentType.find("text/css") == 0 || contentType.find("application/javascript") == 0) {
                // CSS and JS - cache for 1 hour
                cacheControl = "public, max-age=3600";
            } else {
                // HTML and other files - cache for 5 minutes
                cacheControl = "public, max-age=300";
            }
            
            std::map<std::string, std::string> headers = {
                {"Content-Type", contentType},
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"},
                {"Cache-Control", cacheControl},
                {"X-Content-Type-Options", "nosniff"} // Security header
            };
            
            // Add ETag for better caching
            std::hash<std::string> hasher;
            std::string etag = "\"" + std::to_string(hasher(content)) + "\"";
            headers["ETag"] = etag;
            
            send_response(sock, 200, content, headers);
            
            ServerLogger::logDebug("[Thread %u] Successfully served %s (%zu bytes, %s)", 
                                 std::this_thread::get_id(), filePath.c_str(), content.size(), contentType.c_str());
                                 
        } catch (const std::exception &ex) {
            ServerLogger::logError("[Thread %u] Failed to serve %s: %s", 
                                 std::this_thread::get_id(), filePath.c_str(), ex.what());
            serve404(sock);
        }
    }

    void UIRoute::serve404(SocketType sock) {
        std::string content = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>404 - Page Not Found | Kolosal</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: "Inter", -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;
            background-color: #0D0E0F;
            color: #FFFFFF;
            height: 100vh;
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            text-align: center;
            padding: 20px;
        }
        
        .container {
            max-width: 600px;
            width: 100%;
        }
        
        .error-code {
            font-size: 8rem;
            font-weight: 700;
            color: #FF2723;
            line-height: 1;
            margin-bottom: 1rem;
        }
        
        .error-title {
            font-size: 2rem;
            font-weight: 600;
            margin-bottom: 1rem;
            color: #FFFFFF;
        }
        
        .error-message {
            font-size: 1.125rem;
            color: #77777E;
            margin-bottom: 2rem;
            line-height: 1.6;
        }
        
        .navigation {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 1rem;
        }
        
        .nav-link {
            display: inline-flex;
            align-items: center;
            gap: 0.5rem;
            padding: 0.75rem 1.5rem;
            background-color: #1C1C1D;
            color: #FFFFFF;
            text-decoration: none;
            border-radius: 8px;
            border: 1px solid #252627;
            font-weight: 500;
            transition: all 0.2s ease;
        }
        
        .nav-link:hover {
            background-color: #252627;
            border-color: #2F3031;
            transform: translateY(-1px);
        }
        
        .nav-link.primary {
            background-color: #0370FF;
            border-color: #0370FF;
        }
        
        .nav-link.primary:hover {
            background-color: #025CE6;
            border-color: #025CE6;
        }
        
        @media (max-width: 768px) {
            .error-code {
                font-size: 6rem;
            }
            
            .error-title {
                font-size: 1.5rem;
            }
            
            .navigation {
                flex-direction: column;
                align-items: center;
            }
            
            .nav-link {
                width: 100%;
                max-width: 250px;
                justify-content: center;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="error-code">404</div>
        <h1 class="error-title">Page Not Found</h1>
        <p class="error-message">
            The page you're looking for doesn't exist or has been moved.
            Let's get you back on track.
        </p>
        <div class="navigation">
            <a href="/" class="nav-link primary">🏠 Go to Playground</a>
            <a href="/discover" class="nav-link">🔍 Model Discovery</a>
            <a href="/retrieve" class="nav-link">📊 Retrieval</a>
            <a href="/model" class="nav-link">🤖 Models</a>
        </div>
    </div>
</body>
</html>)";

        std::map<std::string, std::string> headers = {
            {"Content-Type", "text/html; charset=utf-8"},
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key"}
        };
        
        send_response(sock, 404, content, headers);
        
        ServerLogger::logInfo("[Thread %u] Served 404 page for missing resource", 
                             std::this_thread::get_id());
    }

} // namespace kolosal
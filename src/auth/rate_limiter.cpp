#include "kolosal/auth/rate_limiter.hpp"
#include "kolosal/logger.hpp"
#include <algorithm>

namespace kolosal {
namespace auth {

using TimePoint = std::chrono::steady_clock::time_point;

RateLimiter::RateLimiter() : config_(), lastGlobalCleanup_(std::chrono::steady_clock::now()) {
    ServerLogger::logInfo("Rate limiter initialized with default config - Max requests: %zu, Window: %lld seconds, Enabled: %s",
                          config_.maxRequests, config_.windowSize.count(), config_.enabled ? "true" : "false");
}

RateLimiter::RateLimiter(const Config& config) 
    : config_(config), lastGlobalCleanup_(std::chrono::steady_clock::now()) {
    ServerLogger::logInfo("Rate limiter initialized - Max requests: %zu, Window: %lld seconds, Enabled: %s",
                          config_.maxRequests, config_.windowSize.count(), config_.enabled ? "true" : "false");
}

RateLimiter::RateLimitResult RateLimiter::checkRateLimit(const std::string& clientIP) {
    if (!config_.enabled) {
        return RateLimiter::RateLimitResult{true, 0, config_.maxRequests, std::chrono::seconds::zero()};
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Perform global cleanup periodically
    auto now = std::chrono::steady_clock::now();
    if (now - lastGlobalCleanup_ > std::chrono::minutes(1)) {
        performPeriodicCleanup();
        lastGlobalCleanup_ = now;
    }
    
    auto& clientData = clients_[clientIP];
    auto windowStart = now - config_.windowSize;
    
    // Remove requests outside the current window
    auto& requests = clientData.requests;
    requests.erase(
        std::remove_if(requests.begin(), requests.end(),
                      [windowStart](const TimePoint& req) { return req < windowStart; }),
        requests.end()
    );
    
    size_t currentRequests = requests.size();
    
    // Check if client has exceeded the rate limit
    if (currentRequests >= config_.maxRequests) {
        size_t remaining = 0;
        
        // Calculate reset time based on oldest request in the window
        std::chrono::seconds resetTime = std::chrono::seconds::zero();
        if (!requests.empty()) {
            auto oldestInWindow = requests.front();
            resetTime = std::chrono::duration_cast<std::chrono::seconds>(
                (oldestInWindow + config_.windowSize) - now);
        }
        
        ServerLogger::logWarning("Rate limit exceeded for client %s - Requests: %zu/%zu", 
                                clientIP.c_str(), currentRequests, config_.maxRequests);
        
        return RateLimiter::RateLimitResult{false, currentRequests, remaining, resetTime};
    }
    
    // Add current request timestamp
    requests.push_back(now);
    
    size_t remaining = config_.maxRequests - (currentRequests + 1);
    std::chrono::seconds resetTime = std::chrono::seconds::zero();
    
    if (!clientData.requests.empty()) {
        auto oldestInWindow = clientData.requests.front();
        if (oldestInWindow > windowStart) {
            resetTime = std::chrono::duration_cast<std::chrono::seconds>(
                (oldestInWindow + config_.windowSize) - now);
        }
    }
    
    ServerLogger::logDebug("Rate limit check passed for client %s - Requests: %zu/%zu, Remaining: %zu", 
                          clientIP.c_str(), currentRequests + 1, config_.maxRequests, remaining);
    
    return RateLimiter::RateLimitResult{true, currentRequests + 1, remaining, resetTime};
}

void RateLimiter::updateConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    ServerLogger::logInfo("Rate limiter config updated - Max requests: %zu, Window: %lld seconds, Enabled: %s",
                          config_.maxRequests, config_.windowSize.count(), config_.enabled ? "true" : "false");
}

RateLimiter::Config RateLimiter::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void RateLimiter::clearClient(const std::string& clientIP) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(clientIP);
    ServerLogger::logInfo("Rate limiter cleared data for client: %s", clientIP.c_str());
}

void RateLimiter::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.clear();
    lastGlobalCleanup_ = std::chrono::steady_clock::now();
    ServerLogger::logInfo("Rate limiter reset - All client data cleared");
}

std::unordered_map<std::string, size_t> RateLimiter::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, size_t> stats;
    
    auto now = std::chrono::steady_clock::now();
    auto windowStart = now - config_.windowSize;
    
    for (const auto& pair : clients_) {
        const auto& requests = pair.second.requests;
        size_t activeRequests = 0;
        
        for (const auto& requestTime : requests) {
            if (requestTime >= windowStart) {
                activeRequests++;
            }
        }
        
        if (activeRequests > 0) {
            stats[pair.first] = activeRequests;
        }
    }
    
    return stats;
}

void RateLimiter::cleanupOldRequests(ClientData& data) {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - config_.windowSize;
    
    auto& requests = data.requests;
    requests.erase(
        std::remove_if(requests.begin(), requests.end(),
                      [cutoff](const TimePoint& req) { return req < cutoff; }),
        requests.end()
    );
}

void RateLimiter::performPeriodicCleanup() {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - config_.windowSize;
    
    auto it = clients_.begin();
    while (it != clients_.end()) {
        cleanupOldRequests(it->second);
        
        // Remove client if no recent requests
        if (it->second.requests.empty()) {
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
    
    ServerLogger::logDebug("Rate limiter cleanup completed - Active clients: %zu", clients_.size());
}

} // namespace auth
} // namespace kolosal

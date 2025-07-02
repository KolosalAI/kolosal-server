#include "kolosal/completion_monitor.hpp"
#include "kolosal/logger.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <random>

namespace kolosal
{
    void CompletionMetrics::calculateMetrics()
    {
        if (completedRequests > 0 && totalTurnaroundTime > 0.0)
        {
            // TPS = (Input Tokens + Output Tokens) / Total Turnaround Time (in seconds)
            tps = (totalInputTokens + totalOutputTokens) / (totalTurnaroundTime / 1000.0);

            // Output TPS = Output Tokens / Time to Generate Output Tokens (in seconds)
            if (totalOutputGenerationTime > 0.0)
            {
                outputTps = totalOutputTokens / (totalOutputGenerationTime / 1000.0);
            }

            // Average Time to First Token
            avgTtft = totalTimeToFirstToken / completedRequests;

            // RPS = Completed Requests / Total Turnaround Time (in seconds)
            rps = completedRequests / (totalTurnaroundTime / 1000.0);
        }
        else
        {
            tps = 0.0;
            outputTps = 0.0;
            avgTtft = 0.0;
            rps = 0.0;
        }
    }
    double CompletionRequestMetrics::getTurnaroundTime() const
    {
        if (completionTime > 0 && requestStartTime > 0)
        {
            return static_cast<double>(completionTime - requestStartTime);
        }
        return 0.0;
    }

    double CompletionRequestMetrics::getTimeToFirstToken() const
    {
        if (firstTokenTime > 0 && requestStartTime > 0)
        {
            return static_cast<double>(firstTokenTime - requestStartTime);
        }
        return 0.0;
    }

    double CompletionRequestMetrics::getOutputGenerationTime() const
    {
        if (completionTime > 0 && firstTokenTime > 0)
        {
            return static_cast<double>(completionTime - firstTokenTime);
        }
        return 0.0;
    }
    struct CompletionMonitor::Impl
    {
        std::mutex requestsMutex;
        std::unordered_map<std::string, CompletionRequestMetrics> activeRequests;

        std::mutex metricsMutex;
        std::unordered_map<std::string, CompletionMetrics> engineMetrics;

        std::mt19937 rng;
        std::uniform_int_distribution<int> dist;

        Impl() : rng(std::chrono::steady_clock::now().time_since_epoch().count()),
                 dist(100000, 999999)
        {
            ServerLogger::logInfo("Completion Monitor initialized");
        }

        std::string generateRequestId()
        {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now.time_since_epoch())
                                 .count();
            int randomSuffix = dist(rng);
            return "req_" + std::to_string(timestamp) + "_" + std::to_string(randomSuffix);
        }

        void updateEngineMetrics(const CompletionRequestMetrics &request)
        {
            std::lock_guard<std::mutex> lock(metricsMutex);

            auto &metrics = engineMetrics[request.engineId];
            metrics.modelName = request.modelName;
            metrics.engineId = request.engineId;
            metrics.totalRequests++;

            if (request.completed)
            {
                metrics.completedRequests++;
                metrics.totalInputTokens += request.inputTokens;
                metrics.totalOutputTokens += request.outputTokens;

                double turnaroundTime = request.getTurnaroundTime();
                double ttft = request.getTimeToFirstToken();
                double outputGenTime = request.getOutputGenerationTime();

                metrics.totalTurnaroundTime += turnaroundTime;
                metrics.totalTimeToFirstToken += ttft;
                metrics.totalOutputGenerationTime += outputGenTime;

                metrics.calculateMetrics();
                metrics.lastUpdated = getCurrentTimestamp();

                ServerLogger::logDebug("Updated metrics for engine '%s': %d completed requests, TPS: %.2f",
                                       request.engineId.c_str(), metrics.completedRequests, metrics.tps);
            }
            else if (request.failed)
            {
                metrics.failedRequests++;
                metrics.lastUpdated = getCurrentTimestamp();
            }
        }

        AggregatedCompletionMetrics calculateAggregatedMetrics()
        {
            std::lock_guard<std::mutex> lock(metricsMutex);

            AggregatedCompletionMetrics aggregated;
            aggregated.timestamp = getCurrentTimestamp();

            double totalWeightedTps = 0.0;
            double totalWeightedOutputTps = 0.0;
            double totalWeightedTtft = 0.0;
            double totalWeightedRps = 0.0;
            int totalCompletedForAvg = 0;

            for (const auto &pair : engineMetrics)
            {
                const auto &metrics = pair.second;
                aggregated.perEngineMetrics.push_back(metrics);

                aggregated.totalRequests += metrics.totalRequests;
                aggregated.completedRequests += metrics.completedRequests;
                aggregated.failedRequests += metrics.failedRequests;
                aggregated.totalInputTokens += metrics.totalInputTokens;
                aggregated.totalOutputTokens += metrics.totalOutputTokens;
                aggregated.totalTurnaroundTime += metrics.totalTurnaroundTime;
                aggregated.totalTimeToFirstToken += metrics.totalTimeToFirstToken;
                aggregated.totalOutputGenerationTime += metrics.totalOutputGenerationTime;

                // Weight averages by number of completed requests
                if (metrics.completedRequests > 0)
                {
                    totalWeightedTps += metrics.tps * metrics.completedRequests;
                    totalWeightedOutputTps += metrics.outputTps * metrics.completedRequests;
                    totalWeightedTtft += metrics.avgTtft * metrics.completedRequests;
                    totalWeightedRps += metrics.rps * metrics.completedRequests;
                    totalCompletedForAvg += metrics.completedRequests;
                }
            }

            // Calculate weighted averages
            if (totalCompletedForAvg > 0)
            {
                aggregated.avgTps = totalWeightedTps / totalCompletedForAvg;
                aggregated.avgOutputTps = totalWeightedOutputTps / totalCompletedForAvg;
                aggregated.avgTtft = totalWeightedTtft / totalCompletedForAvg;
                aggregated.avgRps = totalWeightedRps / totalCompletedForAvg;
            }

            return aggregated;
        }

        void cleanupCompletedRequests(int maxAgeMs)
        {
            std::lock_guard<std::mutex> lock(requestsMutex);
            long long currentTime = getCurrentTimeMs();

            auto it = activeRequests.begin();
            while (it != activeRequests.end())
            {
                const auto &request = it->second;
                if ((request.completed || request.failed) &&
                    (currentTime - request.completionTime) > maxAgeMs)
                {
                    it = activeRequests.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        static std::string getCurrentTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()) %
                      1000;

            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
            return oss.str();
        }

        static long long getCurrentTimeMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
        }
    };
    CompletionMonitor::CompletionMonitor() : pImpl(std::make_unique<Impl>()) {}

    CompletionMonitor::~CompletionMonitor() = default;

    CompletionMonitor &CompletionMonitor::getInstance()
    {
        static CompletionMonitor instance;
        return instance;
    }

    std::string CompletionMonitor::startRequest(const std::string &modelName, const std::string &engineId)
    {
        std::lock_guard<std::mutex> lock(pImpl->requestsMutex);

        std::string requestId = pImpl->generateRequestId();
        CompletionRequestMetrics request;
        request.requestId = requestId;
        request.modelName = modelName;
        request.engineId = engineId;
        request.requestStartTime = getCurrentTimeMs();

        pImpl->activeRequests[requestId] = request;

        ServerLogger::logDebug("Started tracking completion request %s for model %s (engine: %s)",
                               requestId.c_str(), modelName.c_str(), engineId.c_str());

        return requestId;
    }

    void CompletionMonitor::recordInputTokens(const std::string &requestId, int tokenCount)
    {
        std::lock_guard<std::mutex> lock(pImpl->requestsMutex);
        auto it = pImpl->activeRequests.find(requestId);
        if (it != pImpl->activeRequests.end())
        {
            it->second.inputTokens = tokenCount;
            ServerLogger::logDebug("Recorded %d input tokens for request %s", tokenCount, requestId.c_str());
        }
    }

    void CompletionMonitor::recordFirstToken(const std::string &requestId)
    {
        std::lock_guard<std::mutex> lock(pImpl->requestsMutex);
        auto it = pImpl->activeRequests.find(requestId);
        if (it != pImpl->activeRequests.end() && it->second.firstTokenTime == 0)
        {
            it->second.firstTokenTime = getCurrentTimeMs();
            ServerLogger::logDebug("Recorded first token time for request %s", requestId.c_str());
        }
    }

    void CompletionMonitor::recordOutputToken(const std::string &requestId)
    {
        std::lock_guard<std::mutex> lock(pImpl->requestsMutex);
        auto it = pImpl->activeRequests.find(requestId);
        if (it != pImpl->activeRequests.end())
        {
            it->second.outputTokens++;

            // Record first token time if this is the first output token
            if (it->second.outputTokens == 1 && it->second.firstTokenTime == 0)
            {
                it->second.firstTokenTime = getCurrentTimeMs();
            }
        }
    }

    void CompletionMonitor::completeRequest(const std::string &requestId)
    {
        std::lock_guard<std::mutex> lock(pImpl->requestsMutex);
        auto it = pImpl->activeRequests.find(requestId);
        if (it != pImpl->activeRequests.end())
        {
            it->second.completed = true;
            it->second.completionTime = getCurrentTimeMs();

            ServerLogger::logInfo("Completed request %s: %d input tokens, %d output tokens, %.2fms turnaround",
                                  requestId.c_str(), it->second.inputTokens, it->second.outputTokens,
                                  it->second.getTurnaroundTime());

            pImpl->updateEngineMetrics(it->second);
        }
    }

    void CompletionMonitor::failRequest(const std::string &requestId)
    {
        std::lock_guard<std::mutex> lock(pImpl->requestsMutex);
        auto it = pImpl->activeRequests.find(requestId);
        if (it != pImpl->activeRequests.end())
        {
            it->second.failed = true;
            it->second.completionTime = getCurrentTimeMs();

            ServerLogger::logWarning("Failed request %s after %.2fms",
                                     requestId.c_str(), it->second.getTurnaroundTime());

            pImpl->updateEngineMetrics(it->second);
        }
    }

    AggregatedCompletionMetrics CompletionMonitor::getCompletionMetrics()
    {
        return pImpl->calculateAggregatedMetrics();
    }

    CompletionMetrics CompletionMonitor::getMetricsForEngine(const std::string &engineId)
    {
        std::lock_guard<std::mutex> lock(pImpl->metricsMutex);
        auto it = pImpl->engineMetrics.find(engineId);
        if (it != pImpl->engineMetrics.end())
        {
            return it->second;
        }

        CompletionMetrics empty;
        empty.engineId = engineId;
        empty.lastUpdated = getCurrentTimestamp();
        return empty;
    }

    std::vector<std::string> CompletionMonitor::getActiveEngines()
    {
        std::lock_guard<std::mutex> lock(pImpl->metricsMutex);
        std::vector<std::string> engines;
        engines.reserve(pImpl->engineMetrics.size());

        for (const auto &pair : pImpl->engineMetrics)
        {
            engines.push_back(pair.first);
        }

        return engines;
    }

    void CompletionMonitor::resetMetrics()
    {
        std::lock_guard<std::mutex> metricsLock(pImpl->metricsMutex);
        std::lock_guard<std::mutex> requestsLock(pImpl->requestsMutex);

        pImpl->engineMetrics.clear();
        pImpl->activeRequests.clear();

        ServerLogger::logInfo("Reset all completion metrics");
    }

    void CompletionMonitor::resetMetricsForEngine(const std::string &engineId)
    {
        std::lock_guard<std::mutex> lock(pImpl->metricsMutex);
        pImpl->engineMetrics.erase(engineId);

        ServerLogger::logInfo("Reset completion metrics for engine %s", engineId.c_str());
    }

    void CompletionMonitor::cleanupOldRequests(int maxAgeSeconds)
    {
        pImpl->cleanupCompletedRequests(maxAgeSeconds * 1000);
    }

    std::string CompletionMonitor::getCurrentTimestamp()
    {
        return Impl::getCurrentTimestamp();
    }

    long long CompletionMonitor::getCurrentTimeMs()
    {
        return Impl::getCurrentTimeMs();
    }

} // namespace kolosal

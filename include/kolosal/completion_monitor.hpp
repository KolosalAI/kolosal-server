#ifndef KOLOSAL_COMPLETION_MONITOR_HPP
#define KOLOSAL_COMPLETION_MONITOR_HPP

#include "export.hpp"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace kolosal
{
    struct CompletionMetrics
    {
        // Per-engine/model metrics
        std::string modelName;
        std::string engineId;
        
        // Request counts
        int totalRequests = 0;
        int completedRequests = 0;
        int failedRequests = 0;
        
        // Token metrics
        int totalInputTokens = 0;
        int totalOutputTokens = 0;
        
        // Time metrics (in milliseconds)
        double totalTurnaroundTime = 0.0;      // Total time from request to completion
        double totalTimeToFirstToken = 0.0;   // Time to generate first token
        double totalOutputGenerationTime = 0.0; // Time to generate all output tokens
        
        // Calculated metrics
        double tps = 0.0;           // (Input + Output Tokens) / Total Turnaround Time
        double outputTps = 0.0;     // Output Tokens / Time to Generate Output Tokens
        double avgTtft = 0.0;       // Average Time to First Token
        double rps = 0.0;           // Completed Requests / Total Turnaround Time
        
        // Last updated timestamp
        std::string lastUpdated;
        
        // Calculate derived metrics
        void calculateMetrics();
    };

    struct AggregatedCompletionMetrics
    {
        // Overall metrics across all engines/models
        int totalRequests = 0;
        int completedRequests = 0;
        int failedRequests = 0;
        
        int totalInputTokens = 0;
        int totalOutputTokens = 0;
        
        double totalTurnaroundTime = 0.0;
        double totalTimeToFirstToken = 0.0;
        double totalOutputGenerationTime = 0.0;
        
        double avgTps = 0.0;
        double avgOutputTps = 0.0;
        double avgTtft = 0.0;
        double avgRps = 0.0;
        
        std::vector<CompletionMetrics> perEngineMetrics;
        std::string timestamp;
    };    // Structure to track individual completion requests for monitoring
    struct CompletionRequestMetrics
    {
        std::string requestId;
        std::string modelName;
        std::string engineId;
        
        int inputTokens = 0;
        int outputTokens = 0;
        
        // Timestamps (in milliseconds since epoch)
        long long requestStartTime = 0;
        long long firstTokenTime = 0;
        long long completionTime = 0;
        
        bool completed = false;
        bool failed = false;
        
        // Calculate timing metrics
        double getTurnaroundTime() const;
        double getTimeToFirstToken() const;
        double getOutputGenerationTime() const;
    };    class KOLOSAL_SERVER_API CompletionMonitor
    {
    public:
        CompletionMonitor();
        ~CompletionMonitor();

        // Singleton instance
        static CompletionMonitor& getInstance();

        // Request lifecycle tracking
        std::string startRequest(const std::string& modelName, const std::string& engineId = "default");
        void recordInputTokens(const std::string& requestId, int tokenCount);
        void recordFirstToken(const std::string& requestId);
        void recordOutputToken(const std::string& requestId);
        void completeRequest(const std::string& requestId);
        void failRequest(const std::string& requestId);

        // Metrics retrieval
        AggregatedCompletionMetrics getCompletionMetrics();
        CompletionMetrics getMetricsForEngine(const std::string& engineId);
        std::vector<std::string> getActiveEngines();

        // Reset and management
        void resetMetrics();
        void resetMetricsForEngine(const std::string& engineId);
        
        // Cleanup old completed requests (older than specified seconds)
        void cleanupOldRequests(int maxAgeSeconds = 3600);

        // Get current timestamp in ISO 8601 format
        static std::string getCurrentTimestamp();

        // Get current time in milliseconds since epoch
        static long long getCurrentTimeMs();

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

} // namespace kolosal

#endif // KOLOSAL_COMPLETION_MONITOR_HPP

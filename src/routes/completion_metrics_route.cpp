#include "kolosal/routes/completion_metrics_route.hpp"
#include "kolosal/server_api.hpp"
#include "kolosal/logger.hpp"
#include "kolosal/utils.hpp"
#include <json.hpp>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace kolosal
{    CompletionMetricsRoute::CompletionMetricsRoute()
        : monitor_(&CompletionMonitor::getInstance())
    {
        ServerLogger::logInfo("Completion Metrics Route initialized");
    }

    CompletionMetricsRoute::~CompletionMetricsRoute() = default;

    bool CompletionMetricsRoute::match(const std::string &method, const std::string &path)
    {
        return method == "GET" && (path == "/completion-metrics" ||
                                   path == "/v1/completion-metrics" ||
                                   path == "/completion/metrics" ||
                                   path.find("/completion-metrics/") == 0 // For engine-specific metrics
                                  );
    }    void CompletionMetricsRoute::handle(SocketType sock, const std::string &body)
    {
        try
        {
            ServerLogger::logInfo("Received completion metrics request");

            // Get completion metrics
            AggregatedCompletionMetrics metrics = monitor_->getCompletionMetrics();
            std::string response = formatMetricsAsJson(metrics);
            send_response(sock, 200, response);
        }
        catch (const std::exception &e)
        {
            ServerLogger::logError("Error in completion metrics route: %s", e.what());
            std::string errorResponse = R"({"error": "Internal server error", "message": ")" + std::string(e.what()) + R"("})";
            send_response(sock, 500, errorResponse);
        }
    }

    std::string CompletionMetricsRoute::formatMetricsAsJson(const AggregatedCompletionMetrics &metrics)
    {
        std::ostringstream json;
        json << std::fixed << std::setprecision(3);

        json << "{\n";
        json << "  \"completion_metrics\": {\n";
        json << "    \"summary\": {\n";
        json << "      \"total_requests\": " << metrics.totalRequests << ",\n";
        json << "      \"completed_requests\": " << metrics.completedRequests << ",\n";
        json << "      \"failed_requests\": " << metrics.failedRequests << ",\n";
        json << "      \"success_rate_percent\": " << (metrics.totalRequests > 0 ? static_cast<double>(metrics.completedRequests) / metrics.totalRequests * 100.0 : 0.0) << ",\n";
        json << "      \"total_input_tokens\": " << metrics.totalInputTokens << ",\n";
        json << "      \"total_output_tokens\": " << metrics.totalOutputTokens << ",\n";
        json << "      \"total_tokens\": " << (metrics.totalInputTokens + metrics.totalOutputTokens) << ",\n";
        json << "      \"average_tps\": " << metrics.avgTps << ",\n";
        json << "      \"average_output_tps\": " << metrics.avgOutputTps << ",\n";
        json << "      \"average_ttft_ms\": " << metrics.avgTtft << ",\n";
        json << "      \"average_rps\": " << metrics.avgRps << ",\n";
        json << "      \"total_turnaround_time_ms\": " << metrics.totalTurnaroundTime << ",\n";
        json << "      \"total_output_generation_time_ms\": " << metrics.totalOutputGenerationTime << "\n";
        json << "    },\n";

        json << "    \"per_engine_metrics\": [\n";
        for (size_t i = 0; i < metrics.perEngineMetrics.size(); ++i)
        {
            const auto &engine = metrics.perEngineMetrics[i];
            json << "      {\n";
            json << "        \"engine_id\": \"" << engine.engineId << "\",\n";
            json << "        \"model_name\": \"" << engine.modelName << "\",\n";
            json << "        \"requests\": {\n";
            json << "          \"total\": " << engine.totalRequests << ",\n";
            json << "          \"completed\": " << engine.completedRequests << ",\n";
            json << "          \"failed\": " << engine.failedRequests << ",\n";
            json << "          \"success_rate_percent\": " << (engine.totalRequests > 0 ? static_cast<double>(engine.completedRequests) / engine.totalRequests * 100.0 : 0.0) << "\n";
            json << "        },\n";
            json << "        \"tokens\": {\n";
            json << "          \"input_total\": " << engine.totalInputTokens << ",\n";
            json << "          \"output_total\": " << engine.totalOutputTokens << ",\n";
            json << "          \"total\": " << (engine.totalInputTokens + engine.totalOutputTokens) << ",\n";
            json << "          \"average_input_per_request\": " << (engine.completedRequests > 0 ? static_cast<double>(engine.totalInputTokens) / engine.completedRequests : 0.0) << ",\n";
            json << "          \"average_output_per_request\": " << (engine.completedRequests > 0 ? static_cast<double>(engine.totalOutputTokens) / engine.completedRequests : 0.0) << "\n";
            json << "        },\n";
            json << "        \"performance\": {\n";
            json << "          \"tps\": " << engine.tps << ",\n";
            json << "          \"output_tps\": " << engine.outputTps << ",\n";
            json << "          \"average_ttft_ms\": " << engine.avgTtft << ",\n";
            json << "          \"rps\": " << engine.rps << ",\n";
            json << "          \"average_turnaround_time_ms\": " << (engine.completedRequests > 0 ? engine.totalTurnaroundTime / engine.completedRequests : 0.0) << ",\n";
            json << "          \"average_output_generation_time_ms\": " << (engine.completedRequests > 0 ? engine.totalOutputGenerationTime / engine.completedRequests : 0.0) << "\n";
            json << "        },\n";
            json << "        \"last_updated\": \"" << engine.lastUpdated << "\"\n";
            json << "      }";

            if (i < metrics.perEngineMetrics.size() - 1)
            {
                json << ",";
            }
            json << "\n";
        }
        json << "    ],\n";

        json << "    \"metadata\": {\n";
        json << "      \"version\": \"1.0\",\n";
        json << "      \"server\": \"kolosal-server\",\n";
        json << "      \"active_engines\": " << metrics.perEngineMetrics.size() << ",\n";
        json << "      \"metrics_descriptions\": {\n";
        json << "        \"tps\": \"Tokens per second: (Input Tokens + Output Tokens) / Total Turnaround Time\",\n";
        json << "        \"output_tps\": \"Output tokens per second: Output Tokens / Output Generation Time\",\n";
        json << "        \"ttft\": \"Time to first token in milliseconds\",\n";
        json << "        \"rps\": \"Requests per second: Completed Requests / Total Turnaround Time\"\n";
        json << "      }\n";
        json << "    }\n";
        json << "  },\n";
        json << "  \"timestamp\": \"" << metrics.timestamp << "\"\n";
        json << "}";

        return json.str();
    }

    std::string CompletionMetricsRoute::formatEngineMetricsAsJson(const CompletionMetrics &metrics)
    {
        std::ostringstream json;
        json << std::fixed << std::setprecision(3);

        json << "{\n";
        json << "  \"engine_metrics\": {\n";
        json << "    \"engine_id\": \"" << metrics.engineId << "\",\n";
        json << "    \"model_name\": \"" << metrics.modelName << "\",\n";
        json << "    \"requests\": {\n";
        json << "      \"total\": " << metrics.totalRequests << ",\n";
        json << "      \"completed\": " << metrics.completedRequests << ",\n";
        json << "      \"failed\": " << metrics.failedRequests << ",\n";
        json << "      \"success_rate_percent\": " << (metrics.totalRequests > 0 ? static_cast<double>(metrics.completedRequests) / metrics.totalRequests * 100.0 : 0.0) << "\n";
        json << "    },\n";
        json << "    \"tokens\": {\n";
        json << "      \"input_total\": " << metrics.totalInputTokens << ",\n";
        json << "      \"output_total\": " << metrics.totalOutputTokens << ",\n";
        json << "      \"total\": " << (metrics.totalInputTokens + metrics.totalOutputTokens) << ",\n";
        json << "      \"average_input_per_request\": " << (metrics.completedRequests > 0 ? static_cast<double>(metrics.totalInputTokens) / metrics.completedRequests : 0.0) << ",\n";
        json << "      \"average_output_per_request\": " << (metrics.completedRequests > 0 ? static_cast<double>(metrics.totalOutputTokens) / metrics.completedRequests : 0.0) << "\n";
        json << "    },\n";
        json << "    \"performance\": {\n";
        json << "      \"tps\": " << metrics.tps << ",\n";
        json << "      \"output_tps\": " << metrics.outputTps << ",\n";
        json << "      \"average_ttft_ms\": " << metrics.avgTtft << ",\n";
        json << "      \"rps\": " << metrics.rps << ",\n";
        json << "      \"average_turnaround_time_ms\": " << (metrics.completedRequests > 0 ? metrics.totalTurnaroundTime / metrics.completedRequests : 0.0) << ",\n";
        json << "      \"average_output_generation_time_ms\": " << (metrics.completedRequests > 0 ? metrics.totalOutputGenerationTime / metrics.completedRequests : 0.0) << "\n";
        json << "    },\n";
        json << "    \"timing_totals\": {\n";
        json << "      \"total_turnaround_time_ms\": " << metrics.totalTurnaroundTime << ",\n";
        json << "      \"total_time_to_first_token_ms\": " << metrics.totalTimeToFirstToken << ",\n";
        json << "      \"total_output_generation_time_ms\": " << metrics.totalOutputGenerationTime << "\n";
        json << "    },\n";
        json << "    \"last_updated\": \"" << metrics.lastUpdated << "\"\n";
        json << "  },\n";
        json << "  \"timestamp\": \"" << CompletionMonitor::getCurrentTimestamp() << "\"\n";
        json << "}";

        return json.str();
    }

} // namespace kolosal

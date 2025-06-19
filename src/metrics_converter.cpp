#include "kolosal/metrics_converter.hpp"
#include "kolosal/system_monitor.hpp"

namespace kolosal
{
    namespace utils
    {

        SystemMetricsResponseModel convertToSystemMetricsResponse(const SystemMetrics &metrics, bool gpuMonitoringAvailable)
        {
            SystemMetricsResponseModel response;

            response.timestamp = metrics.timestamp;

            // Convert CPU info
            if (metrics.cpuUsage >= 0)
            {
                response.cpu.usage_percent = metrics.cpuUsage;
            }

            // Convert memory info
            response.memory.total_bytes = metrics.totalRAM;
            response.memory.used_bytes = metrics.usedRAM;
            response.memory.free_bytes = metrics.freeRAM;
            response.memory.total_formatted = SystemMonitor::formatBytes(metrics.totalRAM);
            response.memory.used_formatted = SystemMonitor::formatBytes(metrics.usedRAM);
            response.memory.free_formatted = SystemMonitor::formatBytes(metrics.freeRAM);
            if (metrics.ramUtilization >= 0)
            {
                response.memory.utilization_percent = metrics.ramUtilization;
            }

            // Convert GPU info
            response.gpus.clear();
            for (const auto &gpu : metrics.gpus)
            {
                GpuInfoDto gpuDto;
                gpuDto.id = gpu.id;
                gpuDto.name = gpu.name;
                gpuDto.vendor = gpu.vendor;

                if (gpu.utilization >= 0)
                {
                    gpuDto.utilization_percent = gpu.utilization;
                }
                if (gpu.memoryUtilization >= 0)
                {
                    gpuDto.memory_utilization_percent = gpu.memoryUtilization;
                }

                gpuDto.total_memory_bytes = gpu.totalMemory;
                gpuDto.used_memory_bytes = gpu.usedMemory;
                gpuDto.free_memory_bytes = gpu.freeMemory;
                gpuDto.total_memory_formatted = SystemMonitor::formatBytes(gpu.totalMemory);
                gpuDto.used_memory_formatted = SystemMonitor::formatBytes(gpu.usedMemory);
                gpuDto.free_memory_formatted = SystemMonitor::formatBytes(gpu.freeMemory);

                if (gpu.temperature >= 0)
                {
                    gpuDto.temperature_celsius = gpu.temperature;
                }
                if (gpu.powerUsage >= 0)
                {
                    gpuDto.power_usage_watts = gpu.powerUsage;
                }

                // Add driver version only for the first GPU to avoid redundancy
                if (gpu.id == 0 && !gpu.driverVersion.empty())
                {
                    gpuDto.driver_version = gpu.driverVersion;
                }

                response.gpus.push_back(gpuDto);
            }

            response.gpu_monitoring_available = gpuMonitoringAvailable;

            // Create summary statistics
            double totalGpuUtilization = 0.0;
            double totalVramUtilization = 0.0;
            int validGpuReadings = 0;
            int validVramReadings = 0;

            for (const auto &gpu : metrics.gpus)
            {
                if (gpu.utilization >= 0)
                {
                    totalGpuUtilization += gpu.utilization;
                    validGpuReadings++;
                }
                if (gpu.memoryUtilization >= 0)
                {
                    totalVramUtilization += gpu.memoryUtilization;
                    validVramReadings++;
                }
            }

            if (metrics.cpuUsage >= 0)
            {
                response.summary.cpu_usage_percent = metrics.cpuUsage;
            }
            if (metrics.ramUtilization >= 0)
            {
                response.summary.ram_utilization_percent = metrics.ramUtilization;
            }
            response.summary.gpu_count = static_cast<int>(metrics.gpus.size());
            if (validGpuReadings > 0)
            {
                response.summary.average_gpu_utilization_percent = totalGpuUtilization / validGpuReadings;
            }
            if (validVramReadings > 0)
            {
                response.summary.average_vram_utilization_percent = totalVramUtilization / validVramReadings;
            }

            // Set metadata
            response.metadata.version = "1.0";
            response.metadata.server = "kolosal-server";
            response.metadata.monitoring_capabilities = nlohmann::json{
                {"cpu", true},
                {"memory", true},
                {"gpu", gpuMonitoringAvailable}};

            return response;
        }

        CompletionMetricsResponseModel convertToCompletionMetricsResponse(const AggregatedCompletionMetrics &metrics)
        {
            CompletionMetricsResponseModel response;

            response.timestamp = metrics.timestamp;

            // Convert summary
            response.summary.total_requests = metrics.totalRequests;
            response.summary.completed_requests = metrics.completedRequests;
            response.summary.failed_requests = metrics.failedRequests;
            response.summary.success_rate_percent = metrics.totalRequests > 0 ? static_cast<double>(metrics.completedRequests) / metrics.totalRequests * 100.0 : 0.0;
            response.summary.total_input_tokens = metrics.totalInputTokens;
            response.summary.total_output_tokens = metrics.totalOutputTokens;
            response.summary.total_tokens = metrics.totalInputTokens + metrics.totalOutputTokens;
            response.summary.average_tps = metrics.avgTps;
            response.summary.average_output_tps = metrics.avgOutputTps;
            response.summary.average_ttft_ms = metrics.avgTtft;
            response.summary.average_rps = metrics.avgRps;
            response.summary.total_turnaround_time_ms = metrics.totalTurnaroundTime;
            response.summary.total_output_generation_time_ms = metrics.totalOutputGenerationTime;

            // Convert per-engine metrics
            response.per_engine_metrics.clear();
            for (const auto &engine : metrics.perEngineMetrics)
            {
                EngineCompletionMetricsDto engineDto;
                engineDto.engine_id = engine.engineId;
                engineDto.model_name = engine.modelName;

                // Requests
                engineDto.requests.total = engine.totalRequests;
                engineDto.requests.completed = engine.completedRequests;
                engineDto.requests.failed = engine.failedRequests;
                engineDto.requests.success_rate_percent = engine.totalRequests > 0 ? static_cast<double>(engine.completedRequests) / engine.totalRequests * 100.0 : 0.0;

                // Tokens
                engineDto.tokens.input_total = engine.totalInputTokens;
                engineDto.tokens.output_total = engine.totalOutputTokens;
                engineDto.tokens.total = engine.totalInputTokens + engine.totalOutputTokens;
                engineDto.tokens.average_input_per_request = engine.completedRequests > 0 ? static_cast<double>(engine.totalInputTokens) / engine.completedRequests : 0.0;
                engineDto.tokens.average_output_per_request = engine.completedRequests > 0 ? static_cast<double>(engine.totalOutputTokens) / engine.completedRequests : 0.0;

                // Performance
                engineDto.performance.tps = engine.tps;
                engineDto.performance.output_tps = engine.outputTps;
                engineDto.performance.average_ttft_ms = engine.avgTtft;
                engineDto.performance.rps = engine.rps;
                engineDto.performance.average_turnaround_time_ms = engine.completedRequests > 0 ? engine.totalTurnaroundTime / engine.completedRequests : 0.0;
                engineDto.performance.average_output_generation_time_ms = engine.completedRequests > 0 ? engine.totalOutputGenerationTime / engine.completedRequests : 0.0;

                engineDto.last_updated = engine.lastUpdated;

                response.per_engine_metrics.push_back(engineDto);
            }

            // Set metadata
            response.metadata.version = "1.0";
            response.metadata.server = "kolosal-server";
            response.metadata.active_engines = static_cast<int>(metrics.perEngineMetrics.size());
            response.metadata.metrics_descriptions = nlohmann::json{
                {"tps", "Tokens per second: (Input Tokens + Output Tokens) / Total Turnaround Time"},
                {"output_tps", "Output tokens per second: Output Tokens / Output Generation Time"},
                {"ttft", "Time to first token in milliseconds"},
                {"rps", "Requests per second: Completed Requests / Total Turnaround Time"}};

            return response;
        }

        EngineMetricsResponseModel convertToEngineMetricsResponse(const CompletionMetrics &metrics)
        {
            EngineMetricsResponseModel response;

            response.engine_id = metrics.engineId;
            response.model_name = metrics.modelName;
            response.timestamp = CompletionMonitor::getCurrentTimestamp();

            // Requests
            response.requests.total = metrics.totalRequests;
            response.requests.completed = metrics.completedRequests;
            response.requests.failed = metrics.failedRequests;
            response.requests.success_rate_percent = metrics.totalRequests > 0 ? static_cast<double>(metrics.completedRequests) / metrics.totalRequests * 100.0 : 0.0;

            // Tokens
            response.tokens.input_total = metrics.totalInputTokens;
            response.tokens.output_total = metrics.totalOutputTokens;
            response.tokens.total = metrics.totalInputTokens + metrics.totalOutputTokens;
            response.tokens.average_input_per_request = metrics.completedRequests > 0 ? static_cast<double>(metrics.totalInputTokens) / metrics.completedRequests : 0.0;
            response.tokens.average_output_per_request = metrics.completedRequests > 0 ? static_cast<double>(metrics.totalOutputTokens) / metrics.completedRequests : 0.0;

            // Performance
            response.performance.tps = metrics.tps;
            response.performance.output_tps = metrics.outputTps;
            response.performance.average_ttft_ms = metrics.avgTtft;
            response.performance.rps = metrics.rps;
            response.performance.average_turnaround_time_ms = metrics.completedRequests > 0 ? metrics.totalTurnaroundTime / metrics.completedRequests : 0.0;
            response.performance.average_output_generation_time_ms = metrics.completedRequests > 0 ? metrics.totalOutputGenerationTime / metrics.completedRequests : 0.0;

            // Timing totals
            response.timing_totals.total_turnaround_time_ms = metrics.totalTurnaroundTime;
            response.timing_totals.total_time_to_first_token_ms = metrics.totalTimeToFirstToken;
            response.timing_totals.total_output_generation_time_ms = metrics.totalOutputGenerationTime;
            response.last_updated = metrics.lastUpdated;

            return response;
        }

        CombinedMetricsResponseModel convertToCombinedMetricsResponse(
            const SystemMetrics &systemMetrics,
            bool gpuMonitoringAvailable,
            const AggregatedCompletionMetrics &completionMetrics)
        {

            CombinedMetricsResponseModel response;

            // Convert system metrics
            response.system_metrics = convertToSystemMetricsResponse(systemMetrics, gpuMonitoringAvailable);

            // Convert completion metrics
            response.completion_metrics = convertToCompletionMetricsResponse(completionMetrics);

            // Use the most recent timestamp
            response.timestamp = systemMetrics.timestamp;

            return response;
        }

    } // namespace utils
} // namespace kolosal

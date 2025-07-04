#pragma once

#include "kolosal/models/system_metrics_response_model.hpp"
#include "kolosal/models/completion_metrics_response_model.hpp"
#include "kolosal/models/combined_metrics_response_model.hpp"
#include "kolosal/system_monitor.hpp"
#include "kolosal/completion_monitor.hpp"

namespace kolosal {
    namespace utils {

        // Convert SystemMetrics to SystemMetricsResponseModel
        SystemMetricsResponseModel convertToSystemMetricsResponse(const SystemMetrics& metrics, bool gpuMonitoringAvailable);

        // Convert AggregatedCompletionMetrics to CompletionMetricsResponseModel
        CompletionMetricsResponseModel convertToCompletionMetricsResponse(const AggregatedCompletionMetrics& metrics);

        // Convert CompletionMetrics to EngineMetricsResponseModel
        EngineMetricsResponseModel convertToEngineMetricsResponse(const CompletionMetrics& metrics);

        // Convert system and completion metrics to combined response
        CombinedMetricsResponseModel convertToCombinedMetricsResponse(
            const SystemMetrics& systemMetrics, 
            bool gpuMonitoringAvailable,
            const AggregatedCompletionMetrics& completionMetrics);

    } // namespace utils
} // namespace kolosal

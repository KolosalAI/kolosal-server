#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <optional>
#include <json.hpp>

namespace kolosal {

    // Request metrics DTO
    struct RequestMetricsDto {
        int total;
        int completed;
        int failed;
        double success_rate_percent;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"total", total},
                {"completed", completed},
                {"failed", failed},
                {"success_rate_percent", success_rate_percent}
            };
        }
    };

    // Token metrics DTO
    struct TokenMetricsDto {
        int input_total;
        int output_total;
        int total;
        double average_input_per_request;
        double average_output_per_request;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"input_total", input_total},
                {"output_total", output_total},
                {"total", total},
                {"average_input_per_request", average_input_per_request},
                {"average_output_per_request", average_output_per_request}
            };
        }
    };

    // Performance metrics DTO
    struct PerformanceMetricsDto {
        double tps;
        double output_tps;
        double average_ttft_ms;
        double rps;
        double average_turnaround_time_ms;
        double average_output_generation_time_ms;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"tps", tps},
                {"output_tps", output_tps},
                {"average_ttft_ms", average_ttft_ms},
                {"rps", rps},
                {"average_turnaround_time_ms", average_turnaround_time_ms},
                {"average_output_generation_time_ms", average_output_generation_time_ms}
            };
        }
    };

    // Timing totals DTO (for individual engine metrics)
    struct TimingTotalsDto {
        double total_turnaround_time_ms;
        double total_time_to_first_token_ms;
        double total_output_generation_time_ms;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"total_turnaround_time_ms", total_turnaround_time_ms},
                {"total_time_to_first_token_ms", total_time_to_first_token_ms},
                {"total_output_generation_time_ms", total_output_generation_time_ms}
            };
        }
    };

    // Per-engine completion metrics DTO
    struct EngineCompletionMetricsDto {
        std::string engine_id;
        std::string model_name;
        RequestMetricsDto requests;
        TokenMetricsDto tokens;
        PerformanceMetricsDto performance;
        std::string last_updated;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"engine_id", engine_id},
                {"model_name", model_name},
                {"requests", requests.to_json()},
                {"tokens", tokens.to_json()},
                {"performance", performance.to_json()},
                {"last_updated", last_updated}
            };
        }
    };

    // Completion metrics summary DTO
    struct CompletionMetricsSummaryDto {
        int total_requests;
        int completed_requests;
        int failed_requests;
        double success_rate_percent;
        int total_input_tokens;
        int total_output_tokens;
        int total_tokens;
        double average_tps;
        double average_output_tps;
        double average_ttft_ms;
        double average_rps;
        double total_turnaround_time_ms;
        double total_output_generation_time_ms;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"total_requests", total_requests},
                {"completed_requests", completed_requests},
                {"failed_requests", failed_requests},
                {"success_rate_percent", success_rate_percent},
                {"total_input_tokens", total_input_tokens},
                {"total_output_tokens", total_output_tokens},
                {"total_tokens", total_tokens},
                {"average_tps", average_tps},
                {"average_output_tps", average_output_tps},
                {"average_ttft_ms", average_ttft_ms},
                {"average_rps", average_rps},
                {"total_turnaround_time_ms", total_turnaround_time_ms},
                {"total_output_generation_time_ms", total_output_generation_time_ms}
            };
        }
    };

    // Completion metrics metadata DTO
    struct CompletionMetricsMetadataDto {
        std::string version;
        std::string server;
        int active_engines;
        nlohmann::json metrics_descriptions;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"version", version},
                {"server", server},
                {"active_engines", active_engines},
                {"metrics_descriptions", metrics_descriptions}
            };
        }
    };    // Main completion metrics response model
    class KOLOSAL_SERVER_API CompletionMetricsResponseModel : public IModel {
    public:
        CompletionMetricsSummaryDto summary;
        std::vector<EngineCompletionMetricsDto> per_engine_metrics;
        CompletionMetricsMetadataDto metadata;
        std::string timestamp;

        // IModel interface implementation
        bool validate() const override {
            // Basic validation - ensure timestamp is not empty
            if (timestamp.empty()) {
                return false;
            }

            // Validate per-engine metrics
            for (const auto& engine : per_engine_metrics) {
                if (engine.engine_id.empty() || engine.model_name.empty()) {
                    return false;
                }
            }

            return true;
        }

        void from_json(const nlohmann::json& j) override {
            timestamp = j.value("timestamp", "");

            // Parse completion_metrics wrapper
            if (j.contains("completion_metrics")) {
                auto metrics_json = j["completion_metrics"];

                // Parse summary
                if (metrics_json.contains("summary")) {
                    auto summary_json = metrics_json["summary"];
                    summary.total_requests = summary_json.value("total_requests", 0);
                    summary.completed_requests = summary_json.value("completed_requests", 0);
                    summary.failed_requests = summary_json.value("failed_requests", 0);
                    summary.success_rate_percent = summary_json.value("success_rate_percent", 0.0);
                    summary.total_input_tokens = summary_json.value("total_input_tokens", 0);
                    summary.total_output_tokens = summary_json.value("total_output_tokens", 0);
                    summary.total_tokens = summary_json.value("total_tokens", 0);
                    summary.average_tps = summary_json.value("average_tps", 0.0);
                    summary.average_output_tps = summary_json.value("average_output_tps", 0.0);
                    summary.average_ttft_ms = summary_json.value("average_ttft_ms", 0.0);
                    summary.average_rps = summary_json.value("average_rps", 0.0);
                    summary.total_turnaround_time_ms = summary_json.value("total_turnaround_time_ms", 0.0);
                    summary.total_output_generation_time_ms = summary_json.value("total_output_generation_time_ms", 0.0);
                }

                // Parse per-engine metrics
                if (metrics_json.contains("per_engine_metrics") && metrics_json["per_engine_metrics"].is_array()) {
                    per_engine_metrics.clear();
                    for (const auto& engine_json : metrics_json["per_engine_metrics"]) {
                        EngineCompletionMetricsDto engine_dto;
                        engine_dto.engine_id = engine_json.value("engine_id", "");
                        engine_dto.model_name = engine_json.value("model_name", "");

                        // Parse requests
                        if (engine_json.contains("requests")) {
                            auto req_json = engine_json["requests"];
                            engine_dto.requests.total = req_json.value("total", 0);
                            engine_dto.requests.completed = req_json.value("completed", 0);
                            engine_dto.requests.failed = req_json.value("failed", 0);
                            engine_dto.requests.success_rate_percent = req_json.value("success_rate_percent", 0.0);
                        }

                        // Parse tokens
                        if (engine_json.contains("tokens")) {
                            auto tokens_json = engine_json["tokens"];
                            engine_dto.tokens.input_total = tokens_json.value("input_total", 0);
                            engine_dto.tokens.output_total = tokens_json.value("output_total", 0);
                            engine_dto.tokens.total = tokens_json.value("total", 0);
                            engine_dto.tokens.average_input_per_request = tokens_json.value("average_input_per_request", 0.0);
                            engine_dto.tokens.average_output_per_request = tokens_json.value("average_output_per_request", 0.0);
                        }

                        // Parse performance
                        if (engine_json.contains("performance")) {
                            auto perf_json = engine_json["performance"];
                            engine_dto.performance.tps = perf_json.value("tps", 0.0);
                            engine_dto.performance.output_tps = perf_json.value("output_tps", 0.0);
                            engine_dto.performance.average_ttft_ms = perf_json.value("average_ttft_ms", 0.0);
                            engine_dto.performance.rps = perf_json.value("rps", 0.0);
                            engine_dto.performance.average_turnaround_time_ms = perf_json.value("average_turnaround_time_ms", 0.0);
                            engine_dto.performance.average_output_generation_time_ms = perf_json.value("average_output_generation_time_ms", 0.0);
                        }

                        engine_dto.last_updated = engine_json.value("last_updated", "");
                        per_engine_metrics.push_back(engine_dto);
                    }
                }

                // Parse metadata
                if (metrics_json.contains("metadata")) {
                    auto metadata_json = metrics_json["metadata"];
                    metadata.version = metadata_json.value("version", "");
                    metadata.server = metadata_json.value("server", "");
                    metadata.active_engines = metadata_json.value("active_engines", 0);
                    if (metadata_json.contains("metrics_descriptions")) {
                        metadata.metrics_descriptions = metadata_json["metrics_descriptions"];
                    }
                }
            }
        }

        nlohmann::json to_json() const override {
            nlohmann::json per_engine_array = nlohmann::json::array();
            for (const auto& engine : per_engine_metrics) {
                per_engine_array.push_back(engine.to_json());
            }

            return nlohmann::json{
                {"completion_metrics", {
                    {"summary", summary.to_json()},
                    {"per_engine_metrics", per_engine_array},
                    {"metadata", metadata.to_json()}
                }},
                {"timestamp", timestamp}
            };
        }
    };    // Individual engine metrics response model (for engine-specific endpoints)
    class KOLOSAL_SERVER_API EngineMetricsResponseModel : public IModel {
    public:
        std::string engine_id;
        std::string model_name;
        RequestMetricsDto requests;
        TokenMetricsDto tokens;
        PerformanceMetricsDto performance;
        TimingTotalsDto timing_totals;
        std::string last_updated;
        std::string timestamp;

        // IModel interface implementation
        bool validate() const override {
            // Basic validation
            if (timestamp.empty() || engine_id.empty() || model_name.empty()) {
                return false;
            }

            return true;
        }

        void from_json(const nlohmann::json& j) override {
            timestamp = j.value("timestamp", "");

            // Parse engine_metrics wrapper
            if (j.contains("engine_metrics")) {
                auto metrics_json = j["engine_metrics"];
                engine_id = metrics_json.value("engine_id", "");
                model_name = metrics_json.value("model_name", "");

                // Parse requests
                if (metrics_json.contains("requests")) {
                    auto req_json = metrics_json["requests"];
                    requests.total = req_json.value("total", 0);
                    requests.completed = req_json.value("completed", 0);
                    requests.failed = req_json.value("failed", 0);
                    requests.success_rate_percent = req_json.value("success_rate_percent", 0.0);
                }

                // Parse tokens
                if (metrics_json.contains("tokens")) {
                    auto tokens_json = metrics_json["tokens"];
                    tokens.input_total = tokens_json.value("input_total", 0);
                    tokens.output_total = tokens_json.value("output_total", 0);
                    tokens.total = tokens_json.value("total", 0);
                    tokens.average_input_per_request = tokens_json.value("average_input_per_request", 0.0);
                    tokens.average_output_per_request = tokens_json.value("average_output_per_request", 0.0);
                }

                // Parse performance
                if (metrics_json.contains("performance")) {
                    auto perf_json = metrics_json["performance"];
                    performance.tps = perf_json.value("tps", 0.0);
                    performance.output_tps = perf_json.value("output_tps", 0.0);
                    performance.average_ttft_ms = perf_json.value("average_ttft_ms", 0.0);
                    performance.rps = perf_json.value("rps", 0.0);
                    performance.average_turnaround_time_ms = perf_json.value("average_turnaround_time_ms", 0.0);
                    performance.average_output_generation_time_ms = perf_json.value("average_output_generation_time_ms", 0.0);
                }

                // Parse timing totals
                if (metrics_json.contains("timing_totals")) {
                    auto timing_json = metrics_json["timing_totals"];
                    timing_totals.total_turnaround_time_ms = timing_json.value("total_turnaround_time_ms", 0.0);
                    timing_totals.total_time_to_first_token_ms = timing_json.value("total_time_to_first_token_ms", 0.0);
                    timing_totals.total_output_generation_time_ms = timing_json.value("total_output_generation_time_ms", 0.0);
                }

                last_updated = metrics_json.value("last_updated", "");
            }
        }

        nlohmann::json to_json() const override {
            return nlohmann::json{
                {"engine_metrics", {
                    {"engine_id", engine_id},
                    {"model_name", model_name},
                    {"requests", requests.to_json()},
                    {"tokens", tokens.to_json()},
                    {"performance", performance.to_json()},
                    {"timing_totals", timing_totals.to_json()},
                    {"last_updated", last_updated}
                }},
                {"timestamp", timestamp}
            };
        }
    };

} // namespace kolosal

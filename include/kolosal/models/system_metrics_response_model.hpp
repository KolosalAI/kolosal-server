#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <optional>
#include <json.hpp>

namespace kolosal {

    // GPU information DTO
    struct GpuInfoDto {
        int id;
        std::string name;
        std::string vendor;
        std::optional<double> utilization_percent;
        std::optional<double> memory_utilization_percent;
        size_t total_memory_bytes;
        size_t used_memory_bytes;
        size_t free_memory_bytes;
        std::string total_memory_formatted;
        std::string used_memory_formatted;
        std::string free_memory_formatted;
        std::optional<double> temperature_celsius;
        std::optional<double> power_usage_watts;
        std::optional<std::string> driver_version;

        nlohmann::json to_json() const {
            nlohmann::json j = {
                {"id", id},
                {"name", name},
                {"vendor", vendor},
                {"utilization", {
                    {"gpu_percent", utilization_percent.has_value() ? nlohmann::json(utilization_percent.value()) : nlohmann::json()},
                    {"memory_percent", memory_utilization_percent.has_value() ? nlohmann::json(memory_utilization_percent.value()) : nlohmann::json()}
                }},
                {"memory", {
                    {"total_bytes", total_memory_bytes},
                    {"used_bytes", used_memory_bytes},
                    {"free_bytes", free_memory_bytes},
                    {"total_formatted", total_memory_formatted},
                    {"used_formatted", used_memory_formatted},
                    {"free_formatted", free_memory_formatted}
                }},
                {"temperature_celsius", temperature_celsius.has_value() ? nlohmann::json(temperature_celsius.value()) : nlohmann::json()},
                {"power_usage_watts", power_usage_watts.has_value() ? nlohmann::json(power_usage_watts.value()) : nlohmann::json()}
            };

            if (driver_version.has_value()) {
                j["driver_version"] = driver_version.value();
            }

            return j;
        }
    };

    // Memory information DTO
    struct MemoryInfoDto {
        size_t total_bytes;
        size_t used_bytes;
        size_t free_bytes;
        std::optional<double> utilization_percent;
        std::string total_formatted;
        std::string used_formatted;
        std::string free_formatted;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"total_bytes", total_bytes},
                {"used_bytes", used_bytes},
                {"free_bytes", free_bytes},
                {"utilization_percent", utilization_percent.has_value() ? nlohmann::json(utilization_percent.value()) : nlohmann::json()},
                {"total_formatted", total_formatted},
                {"used_formatted", used_formatted},
                {"free_formatted", free_formatted}
            };
        }
    };

    // CPU information DTO
    struct CpuInfoDto {
        std::optional<double> usage_percent;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"usage_percent", usage_percent.has_value() ? nlohmann::json(usage_percent.value()) : nlohmann::json()}
            };
        }
    };

    // System metrics summary DTO
    struct SystemMetricsSummaryDto {
        std::optional<double> cpu_usage_percent;
        std::optional<double> ram_utilization_percent;
        int gpu_count;
        std::optional<double> average_gpu_utilization_percent;
        std::optional<double> average_vram_utilization_percent;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"cpu_usage_percent", cpu_usage_percent.has_value() ? nlohmann::json(cpu_usage_percent.value()) : nlohmann::json()},
                {"ram_utilization_percent", ram_utilization_percent.has_value() ? nlohmann::json(ram_utilization_percent.value()) : nlohmann::json()},
                {"gpu_count", gpu_count},
                {"average_gpu_utilization_percent", average_gpu_utilization_percent.has_value() ? nlohmann::json(average_gpu_utilization_percent.value()) : nlohmann::json()},
                {"average_vram_utilization_percent", average_vram_utilization_percent.has_value() ? nlohmann::json(average_vram_utilization_percent.value()) : nlohmann::json()}
            };
        }
    };

    // System metrics metadata DTO
    struct SystemMetricsMetadataDto {
        std::string version;
        std::string server;
        nlohmann::json monitoring_capabilities;

        nlohmann::json to_json() const {
            return nlohmann::json{
                {"version", version},
                {"server", server},
                {"monitoring_capabilities", monitoring_capabilities}
            };
        }
    };    // Main system metrics response model
    class KOLOSAL_SERVER_API SystemMetricsResponseModel : public IModel {
    public:
        std::string timestamp;
        CpuInfoDto cpu;
        MemoryInfoDto memory;
        std::vector<GpuInfoDto> gpus;
        bool gpu_monitoring_available;
        SystemMetricsSummaryDto summary;
        SystemMetricsMetadataDto metadata;

        // IModel interface implementation
        bool validate() const override {
            // Basic validation - ensure timestamp is not empty and gpus vector is properly structured
            if (timestamp.empty()) {
                return false;
            }

            // Validate GPU data if available
            for (const auto& gpu : gpus) {
                if (gpu.name.empty() || gpu.vendor.empty()) {
                    return false;
                }
            }

            return true;
        }

        void from_json(const nlohmann::json& j) override {
            timestamp = j.value("timestamp", "");
            
            // Parse CPU info
            if (j.contains("cpu")) {
                auto cpu_json = j["cpu"];
                if (cpu_json.contains("usage_percent") && !cpu_json["usage_percent"].is_null()) {
                    cpu.usage_percent = cpu_json["usage_percent"];
                }
            }

            // Parse memory info
            if (j.contains("memory")) {
                auto mem_json = j["memory"];
                memory.total_bytes = mem_json.value("total_bytes", 0);
                memory.used_bytes = mem_json.value("used_bytes", 0);
                memory.free_bytes = mem_json.value("free_bytes", 0);
                memory.total_formatted = mem_json.value("total_formatted", "");
                memory.used_formatted = mem_json.value("used_formatted", "");
                memory.free_formatted = mem_json.value("free_formatted", "");
                
                if (mem_json.contains("utilization_percent") && !mem_json["utilization_percent"].is_null()) {
                    memory.utilization_percent = mem_json["utilization_percent"];
                }
            }

            // Parse GPU info
            if (j.contains("gpus") && j["gpus"].is_array()) {
                gpus.clear();
                for (const auto& gpu_json : j["gpus"]) {
                    GpuInfoDto gpu_dto;
                    gpu_dto.id = gpu_json.value("id", 0);
                    gpu_dto.name = gpu_json.value("name", "");
                    gpu_dto.vendor = gpu_json.value("vendor", "");

                    if (gpu_json.contains("utilization")) {
                        auto util_json = gpu_json["utilization"];
                        if (util_json.contains("gpu_percent") && !util_json["gpu_percent"].is_null()) {
                            gpu_dto.utilization_percent = util_json["gpu_percent"];
                        }
                        if (util_json.contains("memory_percent") && !util_json["memory_percent"].is_null()) {
                            gpu_dto.memory_utilization_percent = util_json["memory_percent"];
                        }
                    }

                    if (gpu_json.contains("memory")) {
                        auto mem_json = gpu_json["memory"];
                        gpu_dto.total_memory_bytes = mem_json.value("total_bytes", 0);
                        gpu_dto.used_memory_bytes = mem_json.value("used_bytes", 0);
                        gpu_dto.free_memory_bytes = mem_json.value("free_bytes", 0);
                        gpu_dto.total_memory_formatted = mem_json.value("total_formatted", "");
                        gpu_dto.used_memory_formatted = mem_json.value("used_formatted", "");
                        gpu_dto.free_memory_formatted = mem_json.value("free_formatted", "");
                    }

                    if (gpu_json.contains("temperature_celsius") && !gpu_json["temperature_celsius"].is_null()) {
                        gpu_dto.temperature_celsius = gpu_json["temperature_celsius"];
                    }

                    if (gpu_json.contains("power_usage_watts") && !gpu_json["power_usage_watts"].is_null()) {
                        gpu_dto.power_usage_watts = gpu_json["power_usage_watts"];
                    }

                    if (gpu_json.contains("driver_version")) {
                        gpu_dto.driver_version = gpu_json["driver_version"];
                    }

                    gpus.push_back(gpu_dto);
                }
            }

            gpu_monitoring_available = j.value("gpu_monitoring_available", false);

            // Parse summary
            if (j.contains("summary")) {
                auto summary_json = j["summary"];
                if (summary_json.contains("cpu_usage_percent") && !summary_json["cpu_usage_percent"].is_null()) {
                    summary.cpu_usage_percent = summary_json["cpu_usage_percent"];
                }
                if (summary_json.contains("ram_utilization_percent") && !summary_json["ram_utilization_percent"].is_null()) {
                    summary.ram_utilization_percent = summary_json["ram_utilization_percent"];
                }
                summary.gpu_count = summary_json.value("gpu_count", 0);
                if (summary_json.contains("average_gpu_utilization_percent") && !summary_json["average_gpu_utilization_percent"].is_null()) {
                    summary.average_gpu_utilization_percent = summary_json["average_gpu_utilization_percent"];
                }
                if (summary_json.contains("average_vram_utilization_percent") && !summary_json["average_vram_utilization_percent"].is_null()) {
                    summary.average_vram_utilization_percent = summary_json["average_vram_utilization_percent"];
                }
            }

            // Parse metadata
            if (j.contains("metadata")) {
                auto metadata_json = j["metadata"];
                metadata.version = metadata_json.value("version", "");
                metadata.server = metadata_json.value("server", "");
                if (metadata_json.contains("monitoring_capabilities")) {
                    metadata.monitoring_capabilities = metadata_json["monitoring_capabilities"];
                }
            }
        }

        nlohmann::json to_json() const override {
            nlohmann::json j = {
                {"timestamp", timestamp},
                {"cpu", cpu.to_json()},
                {"memory", memory.to_json()},
                {"gpus", nlohmann::json::array()},
                {"gpu_monitoring_available", gpu_monitoring_available},
                {"summary", summary.to_json()},
                {"metadata", metadata.to_json()}
            };

            // Add GPU information
            for (const auto& gpu : gpus) {
                j["gpus"].push_back(gpu.to_json());
            }

            return j;
        }
    };

} // namespace kolosal

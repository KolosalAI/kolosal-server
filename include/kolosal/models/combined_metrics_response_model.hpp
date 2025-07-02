#pragma once

#include "system_metrics_response_model.hpp"
#include "completion_metrics_response_model.hpp"
#include "../export.hpp"
#include <json.hpp>

namespace kolosal {

    /**
     * @brief Combined metrics response model that includes both system and completion metrics
     * This model is used for endpoints like /metrics and /v1/metrics
     */
    class KOLOSAL_SERVER_API CombinedMetricsResponseModel : public IModel {
    public:
        // System metrics section
        SystemMetricsResponseModel system_metrics;
        
        // Completion metrics section
        CompletionMetricsResponseModel completion_metrics;
        
        // Overall timestamp
        std::string timestamp;

        // IModel interface implementation
        nlohmann::json to_json() const override {
            nlohmann::json j;
            
            // Add system metrics
            j["system_metrics"] = system_metrics.to_json();
            
            // Add completion metrics  
            j["completion_metrics"] = completion_metrics.to_json();
            
            // Add overall timestamp
            j["timestamp"] = timestamp;
            
            return j;
        }

        void from_json(const nlohmann::json& j) override {
            if (j.contains("system_metrics")) {
                system_metrics.from_json(j["system_metrics"]);
            }
            
            if (j.contains("completion_metrics")) {
                completion_metrics.from_json(j["completion_metrics"]);
            }
            
            if (j.contains("timestamp")) {
                timestamp = j.value("timestamp", "");
            }
        }

        bool validate() const override {
            // Validate both system and completion metrics
            return system_metrics.validate() && completion_metrics.validate();
        }
    };

} // namespace kolosal

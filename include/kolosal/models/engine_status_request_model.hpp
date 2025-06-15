#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <json.hpp>

/**
 * @brief Model for engine status request parameters
 * 
 * This model represents the JSON request body for checking engine status.
 * Currently used as a workaround since path parameters aren't accessible in the routing system.
 */
class KOLOSAL_SERVER_API EngineStatusRequest : public IModel {
public:
    // Required field (when using JSON body workaround)
    std::string engine_id;

    bool validate() const override {
        if (engine_id.empty()) {
            return false;
        }
        return true;
    }

    void from_json(const nlohmann::json& j) override {
        // Add basic validation to ensure we have a valid JSON object
        if (!j.is_object()) {
            throw std::runtime_error("Request must be a JSON object");
        }

        // Check for required field
        if (!j.contains("engine_id")) {
            throw std::runtime_error("Missing required field: engine_id is required");
        }

        if (!j["engine_id"].is_string()) {
            throw std::runtime_error("engine_id must be a string");
        }
        j.at("engine_id").get_to(engine_id);
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
            {"engine_id", engine_id}
        };

        return j;
    }
};

#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <json.hpp>

/**
 * @brief Model for remove model request parameters
 * 
 * This model represents the JSON request body for removing a model.
 * Currently used as a workaround since path parameters aren't accessible in the routing system.
 */
class KOLOSAL_SERVER_API RemoveModelRequest : public IModel {
public:
    // Required field (when using JSON body workaround)
    std::string model_id;

    bool validate() const override {
        if (model_id.empty()) {
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
        if (!j.contains("model_id")) {
            throw std::runtime_error("Missing required field: model_id is required");
        }

        if (!j["model_id"].is_string()) {
            throw std::runtime_error("model_id must be a string");
        }
        j.at("model_id").get_to(model_id);
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
            {"model_id", model_id}
        };

        return j;
    }
};

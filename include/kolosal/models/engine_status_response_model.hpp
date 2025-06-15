#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <json.hpp>

/**
 * @brief Model for engine status response
 * 
 * This model represents the JSON response body for engine status queries.
 */
class KOLOSAL_SERVER_API EngineStatusResponse : public IModel {
public:
    std::string engine_id;
    std::string status;      // "loaded", "unloaded"
    bool available;          // true if engine exists, false otherwise
    std::string message;     // Descriptive message about the engine status

    bool validate() const override {
        // Basic validation for response
        return !engine_id.empty() && !status.empty() && !message.empty();
    }

    void from_json(const nlohmann::json& j) override {
        // This would be used if parsing a response JSON
        if (j.contains("engine_id")) j.at("engine_id").get_to(engine_id);
        if (j.contains("status")) j.at("status").get_to(status);
        if (j.contains("available")) j.at("available").get_to(available);
        if (j.contains("message")) j.at("message").get_to(message);
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
            {"engine_id", engine_id},
            {"status", status},
            {"available", available},
            {"message", message}
        };

        return j;
    }
};

/**
 * @brief Model for engine status error response
 * 
 * This model represents error responses for engine status queries.
 */
class KOLOSAL_SERVER_API EngineStatusErrorResponse : public IModel {
public:
    struct ErrorDetails {
        std::string message;
        std::string type;
        std::string param;
        std::string code;

        nlohmann::json to_json() const {
            nlohmann::json j = {
                {"message", message},
                {"type", type},
                {"param", param.empty() ? nullptr : nlohmann::json(param)},
                {"code", code.empty() ? nullptr : nlohmann::json(code)}
            };
            return j;
        }
    } error;

    bool validate() const override {
        return !error.message.empty() && !error.type.empty();
    }

    void from_json(const nlohmann::json& j) override {
        if (j.contains("error")) {
            auto err = j["error"];
            if (err.contains("message")) err.at("message").get_to(error.message);
            if (err.contains("type")) err.at("type").get_to(error.type);
            if (err.contains("param") && !err["param"].is_null()) err.at("param").get_to(error.param);
            if (err.contains("code") && !err["code"].is_null()) err.at("code").get_to(error.code);
        }
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
            {"error", error.to_json()}
        };

        return j;
    }
};

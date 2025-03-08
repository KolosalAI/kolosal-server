#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <json.hpp>

class KOLOSAL_SERVER_API ChatMessage : public IModel {
public:
    std::string role;
    std::string content;

    bool validate() const override {
        return !role.empty();
    }

    void from_json(const nlohmann::json& j) override {
        if (!j.is_object()) {
            throw std::runtime_error("Message must be a JSON object");
        }

        if (!j.contains("role")) {
            throw std::runtime_error("Message must have a role field");
        }

        if (!j["role"].is_string()) {
            throw std::runtime_error("Role must be a string");
        }

        j.at("role").get_to(role);

        // Content can be null in some cases (like function calls), but we're
        // ignoring those for now
        content = ""; // Default to empty
        if (j.contains("content") && !j["content"].is_null()) {
            if (!j["content"].is_string()) {
                throw std::runtime_error("Content must be a string or null");
            }
            j.at("content").get_to(content);
        }
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{ {"role", role}, {"content", content} };
    }
};

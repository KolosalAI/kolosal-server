#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include "model_interface.hpp"
#include <string>
#include <json.hpp>

class ChatMessage : public IModel {
public:
    std::string role;
    std::string content;

    bool validate() const override {
        return !role.empty();
    }

    void from_json(const nlohmann::json& j) override {
        j.at("role").get_to(role);

        // Content can be null in some cases (like function calls), but we're
        // ignoring those for now
        if (j.contains("content") && !j["content"].is_null()) {
            j.at("content").get_to(content);
        }
        else {
            content = "";
        }
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{ {"role", role}, {"content", content} };
    }
};

#endif // CHAT_MESSAGE_HPP

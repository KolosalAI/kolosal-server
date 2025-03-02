#ifndef CHAT_COMPLETION_REQUEST_HPP
#define CHAT_COMPLETION_REQUEST_HPP

#include "model_interface.hpp"
#include "chat_message_model.hpp"
#include <string>
#include <vector>
#include <optional>
#include <json.hpp>

class ChatCompletionRequest : public IModel {
public:
    std::string model;
    std::vector<ChatMessage> messages;
    bool stream = false;
    double temperature = 1.0;
    double top_p = 1.0;
    int n = 1;
    std::optional<int> max_tokens;
    std::optional<double> presence_penalty;
    std::optional<double> frequency_penalty;
    std::optional<std::string> user;
    std::optional<int> seed;

    // We're ignoring the following as requested:
    // - tools
    // - tool_choice
    // - parallel_tool_calls

    bool validate() const override {
        if (model.empty() || messages.empty()) {
            return false;
        }

        for (const auto& msg : messages) {
            if (!msg.validate()) {
                return false;
            }
        }

        return true;
    }

    void from_json(const nlohmann::json& j) override {
        j.at("model").get_to(model);

        auto msgs = j.at("messages");
        for (const auto& msg_json : msgs) {
            ChatMessage msg;
            msg.from_json(msg_json);
            messages.push_back(msg);
        }

        // Optional parameters
        if (j.contains("stream") && !j["stream"].is_null()) {
            j.at("stream").get_to(stream);
        }

        if (j.contains("temperature") && !j["temperature"].is_null()) {
            j.at("temperature").get_to(temperature);
        }

        if (j.contains("top_p") && !j["top_p"].is_null()) {
            j.at("top_p").get_to(top_p);
        }

        if (j.contains("n") && !j["n"].is_null()) {
            j.at("n").get_to(n);
        }

        if (j.contains("max_tokens") && !j["max_tokens"].is_null()) {
            max_tokens = j["max_tokens"].get<int>();
        }

        if (j.contains("presence_penalty") && !j["presence_penalty"].is_null()) {
            presence_penalty = j["presence_penalty"].get<double>();
        }

        if (j.contains("frequency_penalty") && !j["frequency_penalty"].is_null()) {
            frequency_penalty = j["frequency_penalty"].get<double>();
        }

        if (j.contains("user") && !j["user"].is_null()) {
            user = j["user"].get<std::string>();
        }

        if (j.contains("seed") && !j["seed"].is_null()) {
            seed = j["seed"].get<int>();
        }
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
          {"model", model},
          {"stream", stream},
          {"temperature", temperature},
          {"top_p", top_p},
          {"n", n}
        };

        nlohmann::json msgs = nlohmann::json::array();
        for (const auto& msg : messages) {
            msgs.push_back(msg.to_json());
        }
        j["messages"] = msgs;

        if (max_tokens.has_value()) {
            j["max_tokens"] = max_tokens.value();
        }

        if (presence_penalty.has_value()) {
            j["presence_penalty"] = presence_penalty.value();
        }

        if (frequency_penalty.has_value()) {
            j["frequency_penalty"] = frequency_penalty.value();
        }

        if (user.has_value()) {
            j["user"] = user.value();
        }

        if (seed.has_value()) {
            j["seed"] = seed.value();
        }

        return j;
    }
};

#endif // CHAT_COMPLETION_REQUEST_HPP

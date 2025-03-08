#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include "chat_message_model.hpp"
#include <string>
#include <vector>
#include <optional>
#include <json.hpp>

class KOLOSAL_SERVER_API ChatCompletionRequest : public IModel {
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
        // Add basic validation to ensure we have a valid JSON object
        if (!j.is_object()) {
            throw std::runtime_error("Request must be a JSON object");
        }

        // Check for required fields
        if (!j.contains("model") || !j.contains("messages")) {
            throw std::runtime_error("Missing required fields (model, messages)");
        }

        j.at("model").get_to(model);

        auto msgs = j.at("messages");
        if (!msgs.is_array()) {
            throw std::runtime_error("Messages must be an array");
        }

        messages.clear(); // Clear existing messages
        for (const auto& msg_json : msgs) {
            ChatMessage msg;
            msg.from_json(msg_json);
            messages.push_back(msg);
        }

        // Optional parameters with improved error handling
        if (j.contains("stream") && !j["stream"].is_null()) {
            if (!j["stream"].is_boolean()) {
                throw std::runtime_error("Stream must be a boolean");
            }
            j.at("stream").get_to(stream);
        }

        if (j.contains("temperature") && !j["temperature"].is_null()) {
            if (!j["temperature"].is_number()) {
                throw std::runtime_error("Temperature must be a number");
            }
            j.at("temperature").get_to(temperature);
        }

        if (j.contains("top_p") && !j["top_p"].is_null()) {
            if (!j["top_p"].is_number()) {
                throw std::runtime_error("Top_p must be a number");
            }
            j.at("top_p").get_to(top_p);
        }

        if (j.contains("n") && !j["n"].is_null()) {
            if (!j["n"].is_number_integer()) {
                throw std::runtime_error("N must be an integer");
            }
            j.at("n").get_to(n);
        }

        if (j.contains("max_tokens") && !j["max_tokens"].is_null()) {
            if (!j["max_tokens"].is_number_integer()) {
                throw std::runtime_error("Max_tokens must be an integer");
            }
            max_tokens = j["max_tokens"].get<int>();
        }

        if (j.contains("presence_penalty") && !j["presence_penalty"].is_null()) {
            if (!j["presence_penalty"].is_number()) {
                throw std::runtime_error("Presence_penalty must be a number");
            }
            presence_penalty = j["presence_penalty"].get<double>();
        }

        if (j.contains("frequency_penalty") && !j["frequency_penalty"].is_null()) {
            if (!j["frequency_penalty"].is_number()) {
                throw std::runtime_error("Frequency_penalty must be a number");
            }
            frequency_penalty = j["frequency_penalty"].get<double>();
        }

        if (j.contains("user") && !j["user"].is_null()) {
            if (!j["user"].is_string()) {
                throw std::runtime_error("User must be a string");
            }
            user = j["user"].get<std::string>();
        }

        if (j.contains("seed") && !j["seed"].is_null()) {
            if (!j["seed"].is_number_integer()) {
                throw std::runtime_error("Seed must be an integer");
            }
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

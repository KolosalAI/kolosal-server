#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include "chat_message_model.hpp"
#include <string>
#include <vector>
#include <ctime>
#include <json.hpp>

// Represents a single choice in the response
struct ChatCompletionChoice {
    int index;
    ChatMessage message;
    std::string finish_reason;

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"index", index},
            {"message", message.to_json()},
            {"finish_reason", finish_reason}
        };
    }
};

// Usage statistics
struct ChatCompletionUsage {
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"prompt_tokens", prompt_tokens},
            {"completion_tokens", completion_tokens},
            {"total_tokens", total_tokens}
        };
    }
};

class KOLOSAL_SERVER_API ChatCompletionResponse : public IModel {
public:
    std::string id;
    std::string object = "chat.completion";
    int64_t created;
    std::string model;
    std::string system_fingerprint = "fp_4d29efe704";
    std::vector<ChatCompletionChoice> choices;
    ChatCompletionUsage usage;
    std::string error;

    ChatCompletionResponse() {
        created = static_cast<int64_t>(std::time(nullptr));

        // Create a random ID with format "chatcmpl-123..."
        const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string randomPart;
        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        for (int i = 0; i < 24; i++) {
            randomPart += chars[std::rand() % chars.length()];
        }

        id = "chatcmpl-" + randomPart;
    }

    bool validate() const override {
        return !id.empty() && !choices.empty();
    }

    void from_json(const nlohmann::json& j) override {
        // This is primarily used for serialization, not deserialization
    }

    nlohmann::json to_json() const override {
        nlohmann::json j = {
          {"id", id},
          {"object", object},
          {"created", created},
          {"model", model},
          {"system_fingerprint", system_fingerprint},
          {"choices", nlohmann::json::array()}
        };

        for (const auto& choice : choices) {
            j["choices"].push_back(choice.to_json());
        }

        j["usage"] = usage.to_json();

        return j;
    }
};

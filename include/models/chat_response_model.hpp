#ifndef CHAT_COMPLETION_RESPONSE_HPP
#define CHAT_COMPLETION_RESPONSE_HPP

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
};

// Usage statistics
struct ChatCompletionUsage {
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
};

class ChatCompletionResponse : public IModel {
public:
    std::string id;
    std::string object = "chat.completion";
    int64_t created;
    std::string model;
    std::string system_fingerprint = "fp_4d29efe704";
    std::vector<ChatCompletionChoice> choices;
    ChatCompletionUsage usage;

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
          {"system_fingerprint", system_fingerprint}
        };

        nlohmann::json choicesJson = nlohmann::json::array();
        for (const auto& choice : choices) {
            nlohmann::json choiceJson = {
              {"index", choice.index},
              {"message", choice.message.to_json()},
              {"finish_reason", choice.finish_reason}
            };
            choicesJson.push_back(choiceJson);
        }
        j["choices"] = choicesJson;

        j["usage"] = {
          {"prompt_tokens", usage.prompt_tokens},
          {"completion_tokens", usage.completion_tokens},
          {"total_tokens", usage.total_tokens}
        };

        return j;
    }
};

#endif // CHAT_COMPLETION_RESPONSE_HPP

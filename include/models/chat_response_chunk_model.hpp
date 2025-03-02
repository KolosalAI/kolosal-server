#ifndef CHAT_COMPLETION_CHUNK_HPP
#define CHAT_COMPLETION_CHUNK_HPP

#include "model_interface.hpp"
#include <string>
#include <vector>
#include <ctime>
#include <json.hpp>

// For streaming - represents a delta in the response
struct ChatCompletionDelta {
    std::string role;
    std::string content;
};

// For streaming - represents a chunk choice
struct ChatCompletionChunkChoice {
    int index;
    ChatCompletionDelta delta;
    std::string finish_reason;
};

class ChatCompletionChunk : public IModel {
public:
    std::string id;
    std::string object = "chat.completion.chunk";
    int64_t created;
    std::string model;
    std::string system_fingerprint = "fp_4d29efe704";
    std::vector<ChatCompletionChunkChoice> choices;

    ChatCompletionChunk() {
        created = static_cast<int64_t>(std::time(nullptr));
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
            nlohmann::json deltaJson = nlohmann::json::object();

            if (!choice.delta.role.empty()) {
                deltaJson["role"] = choice.delta.role;
            }

            if (!choice.delta.content.empty()) {
                deltaJson["content"] = choice.delta.content;
            }

            nlohmann::json choiceJson = {
              {"index", choice.index},
              {"delta", deltaJson}
            };

            if (!choice.finish_reason.empty()) {
                choiceJson["finish_reason"] = choice.finish_reason;
            }
            else {
                choiceJson["finish_reason"] = nullptr;
            }

            choicesJson.push_back(choiceJson);
        }
        j["choices"] = choicesJson;

        return j;
    }
};

#endif // CHAT_COMPLETION_CHUNK_HPP

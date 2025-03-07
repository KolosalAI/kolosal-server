#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <ctime>
#include <json.hpp>

// For streaming - represents a delta in the response
struct ChatCompletionDelta {
    std::string role;
    std::string content;

    nlohmann::json to_json() const {
        nlohmann::json j = nlohmann::json::object();
        if (!role.empty()) {
            j["role"] = role;
        }
        if (!content.empty()) {
            j["content"] = content;
        }
        return j;
    }
};

// For streaming - represents a chunk choice
struct ChatCompletionChunkChoice {
    int index;
    ChatCompletionDelta delta;
    std::string finish_reason;

    nlohmann::json to_json() const {
        nlohmann::json j = {
            {"index", index},
            {"delta", delta.to_json()}
        };

        if (!finish_reason.empty()) {
            j["finish_reason"] = finish_reason;
        }
        else {
            j["finish_reason"] = nullptr;
        }

        return j;
    }
};

class KOLOSAL_SERVER_API ChatCompletionChunk : public IModel {
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
            choicesJson.push_back(choice.to_json());
        }
        j["choices"] = choicesJson;

        return j;
    }

    // Format as a SSE (Server-Sent Events) message for streaming
    std::string to_sse() const {
        std::string output = "data: ";
        output += this->to_json().dump();
        output += "\n\n";
        return output;
    }
};

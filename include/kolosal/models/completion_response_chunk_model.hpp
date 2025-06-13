#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <ctime>
#include <optional>
#include <json.hpp>

// For streaming - represents a chunk choice
struct CompletionChunkChoice {
    int index;
    std::string text;
    std::optional<std::map<std::string, std::vector<double>>> logprobs;
    std::string finish_reason;

    nlohmann::json to_json() const {
        nlohmann::json j = {
            {"text", text},
            {"index", index}
        };

        if (!finish_reason.empty()) {
            j["finish_reason"] = finish_reason;
        }
        else {
            j["finish_reason"] = nullptr;
        }

        if (logprobs.has_value()) {
            j["logprobs"] = logprobs.value();
        }
        else {
            j["logprobs"] = nullptr;
        }

        return j;
    }
};

class KOLOSAL_SERVER_API CompletionChunk : public IModel {
public:
    std::string id;
    std::string object = "text_completion";
    int64_t created;
    std::string model;
    std::string system_fingerprint = "fp_44709d6fcb";
    std::vector<CompletionChunkChoice> choices;

    CompletionChunk() {
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
          {"system_fingerprint", system_fingerprint},
          {"choices", nlohmann::json::array()}
        };

        for (const auto& choice : choices) {
            j["choices"].push_back(choice.to_json());
        }

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
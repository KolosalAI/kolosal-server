#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <ctime>
#include <optional>
#include <json.hpp>

// Represents a single choice in the completion response
struct CompletionChoice {
    int index;
    std::string text;
    std::optional<std::map<std::string, std::vector<double>>> logprobs;
    std::string finish_reason;

    nlohmann::json to_json() const {
        nlohmann::json j = {
            {"text", text},
            {"index", index},
            {"finish_reason", finish_reason}
        };

        if (logprobs.has_value()) {
            j["logprobs"] = logprobs.value();
        }
        else {
            j["logprobs"] = nullptr;
        }

        return j;
    }
};

// Usage statistics
struct CompletionUsage {
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

class KOLOSAL_SERVER_API CompletionResponse : public IModel {
public:
    std::string id;
    std::string object = "text_completion";
    int64_t created;
    std::string model;
    std::string system_fingerprint = "fp_44709d6fcb";
    std::vector<CompletionChoice> choices;
    CompletionUsage usage;
    std::string error;

    CompletionResponse() {
        created = static_cast<int64_t>(std::time(nullptr));

        // Create a random ID with format "cmpl-123..."
        const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string randomPart;
        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        for (int i = 0; i < 24; i++) {
            randomPart += chars[std::rand() % chars.length()];
        }

        id = "cmpl-" + randomPart;
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
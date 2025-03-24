#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <optional>
#include <json.hpp>
#include <variant>

class KOLOSAL_SERVER_API CompletionRequest : public IModel {
public:
    std::string model;
    std::variant<std::string, std::vector<std::string>> prompt;
    bool stream = false;
    double temperature = 1.0;
    double top_p = 1.0;
    int n = 1;
    std::optional<int> max_tokens;
    std::optional<double> presence_penalty;
    std::optional<double> frequency_penalty;
    std::optional<int> best_of;
    std::optional<bool> echo;
    std::optional<std::string> suffix;
    std::optional<std::map<std::string, int>> logit_bias;
    std::optional<int> logprobs;
    std::optional<std::variant<std::string, std::vector<std::string>>> stop;
    std::optional<std::string> user;
    std::optional<int> seed;

    bool validate() const override {
        if (model.empty()) {
            return false;
        }

        // Check if prompt is valid
        bool hasValidPrompt = false;
        if (std::holds_alternative<std::string>(prompt)) {
            hasValidPrompt = !std::get<std::string>(prompt).empty();
        }
        else if (std::holds_alternative<std::vector<std::string>>(prompt)) {
            hasValidPrompt = !std::get<std::vector<std::string>>(prompt).empty();
        }

        if (!hasValidPrompt) {
            return false;
        }

        return true;
    }

    void from_json(const nlohmann::json& j) override {
        // Add basic validation to ensure we have a valid JSON object
        if (!j.is_object()) {
            throw std::runtime_error("Request must be a JSON object");
        }

        // Check for required fields
        if (!j.contains("model") || !j.contains("prompt")) {
            throw std::runtime_error("Missing required fields (model, prompt)");
        }

        j.at("model").get_to(model);

        // Handle prompt which can be string or array of strings
        auto& p = j.at("prompt");
        if (p.is_string()) {
            prompt = p.get<std::string>();
        }
        else if (p.is_array()) {
            prompt = p.get<std::vector<std::string>>();
        }
        else {
            throw std::runtime_error("Prompt must be a string or array of strings");
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

        if (j.contains("best_of") && !j["best_of"].is_null()) {
            if (!j["best_of"].is_number_integer()) {
                throw std::runtime_error("Best_of must be an integer");
            }
            best_of = j["best_of"].get<int>();
        }

        if (j.contains("echo") && !j["echo"].is_null()) {
            if (!j["echo"].is_boolean()) {
                throw std::runtime_error("Echo must be a boolean");
            }
            echo = j["echo"].get<bool>();
        }

        if (j.contains("suffix") && !j["suffix"].is_null()) {
            if (!j["suffix"].is_string()) {
                throw std::runtime_error("Suffix must be a string");
            }
            suffix = j["suffix"].get<std::string>();
        }

        if (j.contains("logit_bias") && !j["logit_bias"].is_null()) {
            if (!j["logit_bias"].is_object()) {
                throw std::runtime_error("Logit_bias must be an object");
            }
            logit_bias = j["logit_bias"].get<std::map<std::string, int>>();
        }

        if (j.contains("logprobs") && !j["logprobs"].is_null()) {
            if (!j["logprobs"].is_number_integer()) {
                throw std::runtime_error("Logprobs must be an integer");
            }
            logprobs = j["logprobs"].get<int>();
        }

        if (j.contains("stop") && !j["stop"].is_null()) {
            auto& s = j["stop"];
            if (s.is_string()) {
                stop = s.get<std::string>();
            }
            else if (s.is_array()) {
                stop = s.get<std::vector<std::string>>();
            }
            else {
                throw std::runtime_error("Stop must be a string or array of strings");
            }
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

        // Handle prompt which can be string or array
        if (std::holds_alternative<std::string>(prompt)) {
            j["prompt"] = std::get<std::string>(prompt);
        }
        else if (std::holds_alternative<std::vector<std::string>>(prompt)) {
            j["prompt"] = std::get<std::vector<std::string>>(prompt);
        }

        if (max_tokens.has_value()) {
            j["max_tokens"] = max_tokens.value();
        }

        if (presence_penalty.has_value()) {
            j["presence_penalty"] = presence_penalty.value();
        }

        if (frequency_penalty.has_value()) {
            j["frequency_penalty"] = frequency_penalty.value();
        }

        if (best_of.has_value()) {
            j["best_of"] = best_of.value();
        }

        if (echo.has_value()) {
            j["echo"] = echo.value();
        }

        if (suffix.has_value()) {
            j["suffix"] = suffix.value();
        }

        if (logit_bias.has_value()) {
            j["logit_bias"] = logit_bias.value();
        }

        if (logprobs.has_value()) {
            j["logprobs"] = logprobs.value();
        }

        if (stop.has_value()) {
            if (std::holds_alternative<std::string>(*stop)) {
                j["stop"] = std::get<std::string>(*stop);
            }
            else if (std::holds_alternative<std::vector<std::string>>(*stop)) {
                j["stop"] = std::get<std::vector<std::string>>(*stop);
            }
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
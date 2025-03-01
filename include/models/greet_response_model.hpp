#ifndef GREET_RESPONSE_MODEL_HPP
#define GREET_RESPONSE_MODEL_HPP

#include "model_interface.hpp"
#include <string>

class GreetResponseModel : public IModel {
public:
    std::string status;
    std::string message;

    bool validate() const override {
        return !status.empty() && !message.empty();
    }

    void from_json(const nlohmann::json& j) override {
        j.at("status").get_to(status);
        j.at("message").get_to(message);
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{ {"status", status}, {"message", message} };
    }
};

#endif  // GREET_RESPONSE_MODEL_HPP

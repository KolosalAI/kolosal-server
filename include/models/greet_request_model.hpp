#ifndef GREET_REQUEST_MODEL_HPP
#define GREET_REQUEST_MODEL_HPP

#include "model_interface.hpp"
#include <string>

class GreetRequestModel : public IModel {
public:
    std::string name;
    int age;

    bool validate() const override {
        return !name.empty() && age >= 0;
    }

    void from_json(const nlohmann::json& j) override {
        j.at("name").get_to(name);
        j.at("age").get_to(age);
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{ {"name", name}, {"age", age} };
    }
};

#endif  // GREET_REQUEST_MODEL_HPP

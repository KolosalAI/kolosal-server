#ifndef GREET_REQUEST_MODEL_HPP
#define GREET_REQUEST_MODEL_HPP

#include "model_interface.hpp"
#include <string>

class GreetRequestModel : public IModel {
public:
    std::string name;
    int age;
    bool streaming = false;

    bool validate() const override {
        return !name.empty() && age >= 0;
    }

    void from_json(const nlohmann::json& j) override {
        j.at("name").get_to(name);
        j.at("age").get_to(age);
        if (j.contains("streaming")) {
            j.at("streaming").get_to(streaming);
        }
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{
          {"name", name},
          {"age", age},
          {"streaming", streaming}
        };
    }
};

#endif  // GREET_REQUEST_MODEL_HPP
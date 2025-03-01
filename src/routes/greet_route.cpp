#include "routes/greet_route.hpp"
#include "utils.hpp"
#include "models/greet_request_model.hpp"
#include "models/greet_response_model.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

bool GreetRoute::match(const std::string& method, const std::string& path) {
    // This route accepts only POST /greet
    return (method == "POST" && path == "/greet");
}

void GreetRoute::handle(SocketType sock, const std::string& body) {
    try {
        auto j = json::parse(body);
        GreetRequestModel req;
        req.from_json(j);
        if (!req.validate()) {
            throw std::invalid_argument("Invalid request body");
        }

        GreetResponseModel resp;
        resp.status = "success";
        resp.message = "Hello, " + req.name + "! Your age is " +
            std::to_string(req.age);
        json jResp = resp.to_json();
        send_response(sock, 200, jResp.dump());
    }
    catch (const std::exception& ex) {
        json jError = { {"error", std::string("Invalid JSON payload: ") + ex.what()} };
        send_response(sock, 400, jError.dump());
    }
}

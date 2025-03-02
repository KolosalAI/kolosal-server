#include "routes/greet_route.hpp"
#include "utils.hpp"
#include "models/greet_request_model.hpp"
#include "models/greet_response_model.hpp"
#include <json.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <random>
#include <chrono>
#include <thread>

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

        if (req.streaming) {
            std::cout << "Sending streaming response to client" << std::endl;

            // Prepare our simulated tokenized content
            std::string greeting = "Hello, " + req.name + "! Your age is " + std::to_string(req.age) +
                ". Let me tell you more about yourself...";
            std::vector<std::string> tokenizedGreeting;

            // Tokenize the greeting
            size_t start = 0;
            size_t end = 0;
            while ((end = greeting.find(" ", start)) != std::string::npos) {
                tokenizedGreeting.push_back(greeting.substr(start, end - start));
                start = end + 1;
            }
            tokenizedGreeting.push_back(greeting.substr(start));

            // Add some extra tokens
            tokenizedGreeting.push_back("You");
            tokenizedGreeting.push_back("seem");
            tokenizedGreeting.push_back("to");
            tokenizedGreeting.push_back("be");
            tokenizedGreeting.push_back("an");
            tokenizedGreeting.push_back("interesting");
            tokenizedGreeting.push_back("person.");

            // Initialize random number generator for token batching
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> tokensPerChunk(1, 3);

            // Begin the streaming response
            begin_streaming_response(sock, 200);

            // Send tokens in chunks
            size_t currentToken = 0;
            while (currentToken < tokenizedGreeting.size()) {
                // Determine how many tokens to send in this chunk
                int numTokens = tokensPerChunk(gen);
                std::string nextTokens = "";

                for (int i = 0; i < numTokens && currentToken < tokenizedGreeting.size(); i++) {
                    if (!nextTokens.empty()) {
                        nextTokens += " ";
                    }
                    nextTokens += tokenizedGreeting[currentToken++];
                }

                // Create a JSON chunk
                json chunk = {
                  {"status", "streaming"},
                  {"tokens", nextTokens}
                };

                // Check if this is the last batch of tokens
                bool isLastBatch = (currentToken >= tokenizedGreeting.size());

                // Send this chunk
                send_stream_chunk(sock, StreamChunk(chunk.dump(), false));
            }

            // Send the final chunk with completion status
            json finalChunk = {
              {"status", "complete"},
              {"message", "Generation complete"}
            };
            send_stream_chunk(sock, StreamChunk(finalChunk.dump(), true));

        }
        else {
            // Regular (non-streaming) response
            GreetResponseModel resp;
            resp.status = "success";
            resp.message = "Hello, " + req.name + "! Your age is " +
                std::to_string(req.age);
            json jResp = resp.to_json();
            send_response(sock, 200, jResp.dump());
        }
    }
    catch (const std::exception& ex) {
        json jError = { {"error", std::string("Invalid JSON payload: ") + ex.what()} };
        send_response(sock, 400, jError.dump());
    }
}
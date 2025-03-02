#ifndef CHAT_COMPLETIONS_ROUTE_HPP
#define CHAT_COMPLETIONS_ROUTE_HPP

#include "route_interface.hpp"
#include "models/chat_request_model.hpp"
#include <string>

class ChatCompletionsRoute : public IRoute {
public:
	bool match(const std::string& method, const std::string& path) override;
	void handle(SocketType sock, const std::string& body) override;

private:
	// Simulate a basic LLM response
	std::string generateResponse(const ChatCompletionRequest& request);

	// Simple token counter (very basic estimation)
	int countTokens(const std::string& text);
};

#endif // CHAT_COMPLETIONS_ROUTE_HPP

#include "inference_interface.h"
#include <algorithm>
#include <variant>

// Implementation of CompletionParameters::isValid()
bool CompletionParameters::isValid() const {
    // Prompt cannot be empty
    if (prompt.empty()) {
        return false;
    }
    
    // Basic validation of parameters
    if (maxNewTokens <= 0 || minLength < 0) {
        return false;
    }
    
    if (temperature < 0.0f || topP < 0.0f || topP > 1.0f) {
        return false;
    }
    
    return true;
}

// Implementation of ChatCompletionParameters::isValid()
bool ChatCompletionParameters::isValid() const {
    // Must have at least one message
    if (messages.empty()) {
        return false;
    }
    
    // Check that all messages have valid roles and content
    for (const auto& message : messages) {
        // Check if content is empty based on variant type
        bool contentEmpty = false;
        if (std::holds_alternative<std::string>(message.content)) {
            contentEmpty = std::get<std::string>(message.content).empty();
        } else {
            contentEmpty = std::get<std::vector<ContentItem>>(message.content).empty();
        }
        
        if (message.role.empty() || contentEmpty) {
            return false;
        }
        
        // Valid roles are "system", "user", "assistant"
        if (message.role != "system" && message.role != "user" && message.role != "assistant") {
            return false;
        }
    }
    
    // Basic validation of parameters
    if (maxNewTokens <= 0 || minLength < 0) {
        return false;
    }
    
    if (temperature < 0.0f || topP < 0.0f || topP > 1.0f) {
        return false;
    }
    
    return true;
}

// Implementation of EmbeddingParameters::isValid()
bool EmbeddingParameters::isValid() const {
    return !input.empty();
}

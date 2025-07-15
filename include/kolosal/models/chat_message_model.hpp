#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <variant>
#include <json.hpp>

/**
 * @brief Represents image content in a chat message
 */
struct ImageContent {
    std::string type = "image_url";
    struct ImageUrl {
        std::string url;
        std::string detail = "auto"; // "auto", "low", "high"
    } image_url;
    
    ImageContent() = default;
    ImageContent(const std::string& url, const std::string& detail = "auto") {
        image_url.url = url;
        image_url.detail = detail;
    }
};

/**
 * @brief Represents text content in a chat message
 */
struct TextContent {
    std::string type = "text";
    std::string text;
    
    TextContent() = default;
    TextContent(const std::string& text) : text(text) {}
};

/**
 * @brief Message content can be either a simple string or an array of content objects
 */
using MessageContent = std::variant<std::string, std::vector<std::variant<TextContent, ImageContent>>>;

class KOLOSAL_SERVER_API ChatMessage : public IModel {
public:
    std::string role;
    MessageContent content;

    bool validate() const override {
        return !role.empty();
    }

    void from_json(const nlohmann::json& j) override {
        if (!j.is_object()) {
            throw std::runtime_error("Message must be a JSON object");
        }

        if (!j.contains("role")) {
            throw std::runtime_error("Message must have a role field");
        }

        if (!j["role"].is_string()) {
            throw std::runtime_error("Role must be a string");
        }

        j.at("role").get_to(role);

        // Handle content field
        if (j.contains("content") && !j["content"].is_null()) {
            if (j["content"].is_string()) {
                // Simple text content
                content = j["content"].get<std::string>();
            } else if (j["content"].is_array()) {
                // Array of content objects (multimodal)
                std::vector<std::variant<TextContent, ImageContent>> content_array;
                
                for (const auto& item : j["content"]) {
                    if (!item.is_object() || !item.contains("type")) {
                        throw std::runtime_error("Content array items must be objects with 'type' field");
                    }
                    
                    std::string type = item["type"].get<std::string>();
                    
                    if (type == "text") {
                        if (!item.contains("text")) {
                            throw std::runtime_error("Text content must have 'text' field");
                        }
                        TextContent text_content;
                        text_content.text = item["text"].get<std::string>();
                        content_array.push_back(text_content);
                    } else if (type == "image_url") {
                        if (!item.contains("image_url") || !item["image_url"].contains("url")) {
                            throw std::runtime_error("Image content must have 'image_url.url' field");
                        }
                        ImageContent image_content;
                        image_content.image_url.url = item["image_url"]["url"].get<std::string>();
                        if (item["image_url"].contains("detail")) {
                            image_content.image_url.detail = item["image_url"]["detail"].get<std::string>();
                        }
                        content_array.push_back(image_content);
                    } else {
                        throw std::runtime_error("Unsupported content type: " + type);
                    }
                }
                
                content = content_array;
            } else {
                throw std::runtime_error("Content must be a string or array");
            }
        } else {
            // Default to empty string for backward compatibility
            content = std::string("");
        }
    }

    nlohmann::json to_json() const override {
        nlohmann::json result;
        result["role"] = role;
        
        if (std::holds_alternative<std::string>(content)) {
            result["content"] = std::get<std::string>(content);
        } else {
            const auto& content_array = std::get<std::vector<std::variant<TextContent, ImageContent>>>(content);
            nlohmann::json content_json = nlohmann::json::array();
            
            for (const auto& item : content_array) {
                nlohmann::json item_json;
                
                if (std::holds_alternative<TextContent>(item)) {
                    const auto& text_content = std::get<TextContent>(item);
                    item_json["type"] = "text";
                    item_json["text"] = text_content.text;
                } else {
                    const auto& image_content = std::get<ImageContent>(item);
                    item_json["type"] = "image_url";
                    item_json["image_url"]["url"] = image_content.image_url.url;
                    item_json["image_url"]["detail"] = image_content.image_url.detail;
                }
                
                content_json.push_back(item_json);
            }
            
            result["content"] = content_json;
        }
        
        return result;
    }
    
    /**
     * @brief Extract text content from message (backward compatibility)
     * @return Combined text content from all text parts
     */
    std::string getTextContent() const {
        if (std::holds_alternative<std::string>(content)) {
            return std::get<std::string>(content);
        } else {
            const auto& content_array = std::get<std::vector<std::variant<TextContent, ImageContent>>>(content);
            std::string text_result;
            
            for (const auto& item : content_array) {
                if (std::holds_alternative<TextContent>(item)) {
                    const auto& text_content = std::get<TextContent>(item);
                    if (!text_result.empty()) text_result += " ";
                    text_result += text_content.text;
                }
            }
            
            return text_result;
        }
    }
    
    /**
     * @brief Extract image URLs from message
     * @return Vector of image URLs and their details
     */
    std::vector<std::pair<std::string, std::string>> getImageUrls() const {
        std::vector<std::pair<std::string, std::string>> images;
        
        if (std::holds_alternative<std::vector<std::variant<TextContent, ImageContent>>>(content)) {
            const auto& content_array = std::get<std::vector<std::variant<TextContent, ImageContent>>>(content);
            
            for (const auto& item : content_array) {
                if (std::holds_alternative<ImageContent>(item)) {
                    const auto& image_content = std::get<ImageContent>(item);
                    images.push_back({image_content.image_url.url, image_content.image_url.detail});
                }
            }
        }
        
        return images;
    }
    
    /**
     * @brief Check if message contains images
     * @return true if message has any image content
     */
    bool hasImages() const {
        if (std::holds_alternative<std::vector<std::variant<TextContent, ImageContent>>>(content)) {
            const auto& content_array = std::get<std::vector<std::variant<TextContent, ImageContent>>>(content);
            
            for (const auto& item : content_array) {
                if (std::holds_alternative<ImageContent>(item)) {
                    return true;
                }
            }
        }
        
        return false;
    }
};

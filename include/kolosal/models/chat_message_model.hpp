#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <variant>
#include <json.hpp>

// Content item types for multimodal support
struct TextContent {
    std::string type = "text";
    std::string text;

    nlohmann::json to_json() const {
        return nlohmann::json{{"type", type}, {"text", text}};
    }
};

struct ImageUrlContent {
    std::string url;
    std::string detail = "auto"; // "auto", "low", "high"

    nlohmann::json to_json() const {
        return nlohmann::json{{"url", url}, {"detail", detail}};
    }
};

struct ImageContent {
    std::string type = "image_url";
    ImageUrlContent image_url;

    nlohmann::json to_json() const {
        return nlohmann::json{{"type", type}, {"image_url", image_url.to_json()}};
    }
};

using ContentItem = std::variant<TextContent, ImageContent>;

class KOLOSAL_SERVER_API ChatMessage : public IModel {
public:
    std::string role;
    
    // Support both string content and array of content items (OpenAI Vision API)
    std::variant<std::string, std::vector<ContentItem>> content;

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
                // Traditional string content
                content = j["content"].get<std::string>();
            }
            else if (j["content"].is_array()) {
                // Array of content items for multimodal
                std::vector<ContentItem> items;
                for (const auto& item : j["content"]) {
                    if (!item.is_object() || !item.contains("type")) {
                        throw std::runtime_error("Content item must have a type field");
                    }

                    std::string item_type = item["type"];
                    if (item_type == "text") {
                        if (!item.contains("text")) {
                            throw std::runtime_error("Text content item must have text field");
                        }
                        TextContent text_content;
                        text_content.text = item["text"];
                        items.emplace_back(text_content);
                    }
                    else if (item_type == "image_url") {
                        if (!item.contains("image_url") || !item["image_url"].contains("url")) {
                            throw std::runtime_error("Image content item must have image_url.url field");
                        }
                        ImageContent image_content;
                        image_content.image_url.url = item["image_url"]["url"];
                        if (item["image_url"].contains("detail")) {
                            image_content.image_url.detail = item["image_url"]["detail"];
                        }
                        items.emplace_back(image_content);
                    }
                    else {
                        throw std::runtime_error("Unsupported content type: " + item_type);
                    }
                }
                content = items;
            }
            else {
                throw std::runtime_error("Content must be a string or array");
            }
        } else {
            content = std::string(""); // Default to empty string
        }
    }

    nlohmann::json to_json() const override {
        nlohmann::json result = {{"role", role}};
        
        if (std::holds_alternative<std::string>(content)) {
            result["content"] = std::get<std::string>(content);
        } else {
            nlohmann::json content_array = nlohmann::json::array();
            const auto& items = std::get<std::vector<ContentItem>>(content);
            for (const auto& item : items) {
                if (std::holds_alternative<TextContent>(item)) {
                    content_array.push_back(std::get<TextContent>(item).to_json());
                } else if (std::holds_alternative<ImageContent>(item)) {
                    content_array.push_back(std::get<ImageContent>(item).to_json());
                }
            }
            result["content"] = content_array;
        }
        
        return result;
    }

    // Helper methods
    bool hasImages() const {
        if (std::holds_alternative<std::vector<ContentItem>>(content)) {
            const auto& items = std::get<std::vector<ContentItem>>(content);
            for (const auto& item : items) {
                if (std::holds_alternative<ImageContent>(item)) {
                    return true;
                }
            }
        }
        return false;
    }

    std::string getTextContent() const {
        if (std::holds_alternative<std::string>(content)) {
            return std::get<std::string>(content);
        } else {
            // Concatenate all text content items
            std::string result;
            const auto& items = std::get<std::vector<ContentItem>>(content);
            for (const auto& item : items) {
                if (std::holds_alternative<TextContent>(item)) {
                    if (!result.empty()) result += " ";
                    result += std::get<TextContent>(item).text;
                }
            }
            return result;
        }
    }

    std::vector<std::string> getImageUrls() const {
        std::vector<std::string> urls;
        if (std::holds_alternative<std::vector<ContentItem>>(content)) {
            const auto& items = std::get<std::vector<ContentItem>>(content);
            for (const auto& item : items) {
                if (std::holds_alternative<ImageContent>(item)) {
                    urls.push_back(std::get<ImageContent>(item).image_url.url);
                }
            }
        }
        return urls;
    }
};

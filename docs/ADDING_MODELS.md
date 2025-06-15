# Adding New Models

This guide explains how to add new data models to the Kolosal Server for handling API requests and responses.

## Model Architecture

All models in Kolosal Server implement the `IModel` interface for consistent JSON serialization and validation:

```cpp
class IModel {
public:
    virtual bool validate() const = 0;
    virtual void from_json(const nlohmann::json& j) = 0;
    virtual nlohmann::json to_json() const = 0;
    virtual ~IModel() = default;
};
```

## Model Types

### 1. Request Models
Handle incoming API request data and validation.

### 2. Response Models
Format outgoing API response data.

### 3. Internal Models
Represent internal data structures and configurations.

## Step-by-Step Guide

### 1. Create Model Header

Create a new header file in `include/kolosal/models/`:

**File**: `include/kolosal/models/custom_request_model.hpp`

```cpp
#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <optional>
#include <json.hpp>

namespace kolosal {
    
    /**
     * @brief Custom request model for demonstration
     * 
     * This model shows common patterns for request handling including:
     * - Required and optional fields
     * - Different data types
     * - Validation logic
     * - JSON serialization
     */
    class KOLOSAL_SERVER_API CustomRequestModel : public IModel {
    public:
        // Required fields
        std::string operation;                          // Required: operation type
        std::string model_id;                          // Required: model identifier
        
        // Optional fields with defaults
        std::optional<int> max_results;                // Optional: maximum results
        std::optional<float> threshold;                // Optional: threshold value
        std::optional<bool> include_metadata;          // Optional: include metadata flag
        
        // Complex fields
        std::vector<std::string> tags;                 // Array of tags
        std::optional<std::string> format;             // Optional format specification
        
        // Nested object (using another model)
        struct FilterOptions {
            std::optional<std::string> category;
            std::optional<int> min_score;
            std::optional<int> max_score;
            
            nlohmann::json to_json() const {
                nlohmann::json j;
                if (category.has_value()) j["category"] = category.value();
                if (min_score.has_value()) j["min_score"] = min_score.value();
                if (max_score.has_value()) j["max_score"] = max_score.value();
                return j;
            }
            
            void from_json(const nlohmann::json& j) {
                if (j.contains("category")) category = j["category"].get<std::string>();
                if (j.contains("min_score")) min_score = j["min_score"].get<int>();
                if (j.contains("max_score")) max_score = j["max_score"].get<int>();
            }
        };
        
        std::optional<FilterOptions> filters;          // Optional nested object
        
        // Constructors
        CustomRequestModel() = default;
        
        // IModel interface implementation
        bool validate() const override;
        void from_json(const nlohmann::json& j) override;
        nlohmann::json to_json() const override;
        
        // Helper methods
        bool hasFilters() const { return filters.has_value(); }
        int getMaxResults() const { return max_results.value_or(10); }
        float getThreshold() const { return threshold.value_or(0.5f); }
        bool shouldIncludeMetadata() const { return include_metadata.value_or(false); }
    };
    
} // namespace kolosal
```

### 2. Implement Model Logic

Create the implementation file in `src/models/`:

**File**: `src/models/custom_request_model.cpp`

```cpp
#include "kolosal/models/custom_request_model.hpp"
#include <stdexcept>
#include <algorithm>

namespace kolosal {
    
    bool CustomRequestModel::validate() const {
        // Validate required fields
        if (operation.empty()) {
            return false;
        }
        
        if (model_id.empty()) {
            return false;
        }
        
        // Validate operation type
        const std::vector<std::string> validOperations = {
            "search", "classify", "generate", "analyze"
        };
        
        if (std::find(validOperations.begin(), validOperations.end(), operation) == 
            validOperations.end()) {
            return false;
        }
        
        // Validate optional field ranges
        if (max_results.has_value() && max_results.value() <= 0) {
            return false;
        }
        
        if (threshold.has_value() && 
            (threshold.value() < 0.0f || threshold.value() > 1.0f)) {
            return false;
        }
        
        // Validate format if specified
        if (format.has_value()) {
            const std::vector<std::string> validFormats = {
                "json", "text", "xml", "csv"
            };
            
            if (std::find(validFormats.begin(), validFormats.end(), format.value()) == 
                validFormats.end()) {
                return false;
            }
        }
        
        // Validate nested filters
        if (filters.has_value()) {
            const auto& f = filters.value();
            if (f.min_score.has_value() && f.max_score.has_value()) {
                if (f.min_score.value() > f.max_score.value()) {
                    return false;  // Invalid range
                }
            }
        }
        
        // Validate tags
        for (const auto& tag : tags) {
            if (tag.empty() || tag.length() > 50) {
                return false;  // Invalid tag length
            }
        }
        
        return true;
    }
    
    void CustomRequestModel::from_json(const nlohmann::json& j) {
        try {
            // Parse required fields
            if (!j.contains("operation")) {
                throw std::invalid_argument("Missing required field: operation");
            }
            operation = j["operation"].get<std::string>();
            
            if (!j.contains("model_id")) {
                throw std::invalid_argument("Missing required field: model_id");
            }
            model_id = j["model_id"].get<std::string>();
            
            // Parse optional fields
            if (j.contains("max_results")) {
                max_results = j["max_results"].get<int>();
            }
            
            if (j.contains("threshold")) {
                threshold = j["threshold"].get<float>();
            }
            
            if (j.contains("include_metadata")) {
                include_metadata = j["include_metadata"].get<bool>();
            }
            
            if (j.contains("format")) {
                format = j["format"].get<std::string>();
            }
            
            // Parse array fields
            if (j.contains("tags") && j["tags"].is_array()) {
                tags.clear();
                for (const auto& tag : j["tags"]) {
                    tags.push_back(tag.get<std::string>());
                }
            }
            
            // Parse nested object
            if (j.contains("filters") && j["filters"].is_object()) {
                FilterOptions f;
                f.from_json(j["filters"]);
                filters = f;
            }
            
        } catch (const nlohmann::json::exception& ex) {
            throw std::invalid_argument("JSON parsing error: " + std::string(ex.what()));
        }
    }
    
    nlohmann::json CustomRequestModel::to_json() const {
        nlohmann::json j;
        
        // Always include required fields
        j["operation"] = operation;
        j["model_id"] = model_id;
        
        // Include optional fields if present
        if (max_results.has_value()) {
            j["max_results"] = max_results.value();
        }
        
        if (threshold.has_value()) {
            j["threshold"] = threshold.value();
        }
        
        if (include_metadata.has_value()) {
            j["include_metadata"] = include_metadata.value();
        }
        
        if (format.has_value()) {
            j["format"] = format.value();
        }
        
        // Include array fields
        if (!tags.empty()) {
            j["tags"] = tags;
        }
        
        // Include nested object if present
        if (filters.has_value()) {
            j["filters"] = filters.value().to_json();
        }
        
        return j;
    }
    
} // namespace kolosal
```

### 3. Create Response Model

**File**: `include/kolosal/models/custom_response_model.hpp`

```cpp
#pragma once

#include "../export.hpp"
#include "model_interface.hpp"
#include <string>
#include <vector>
#include <ctime>
#include <json.hpp>

namespace kolosal {
    
    /**
     * @brief Response model for custom operations
     */
    class KOLOSAL_SERVER_API CustomResponseModel : public IModel {
    public:
        // Standard response fields
        std::string id;
        std::string object = "custom.response";
        int64_t created;
        std::string model;
        
        // Result data
        struct ResultItem {
            std::string item_id;
            std::string content;
            float score;
            std::vector<std::string> categories;
            
            nlohmann::json to_json() const {
                return nlohmann::json{
                    {"item_id", item_id},
                    {"content", content},
                    {"score", score},
                    {"categories", categories}
                };
            }
        };
        
        std::vector<ResultItem> results;
        
        // Metadata
        struct ResponseMetadata {
            int total_count;
            int filtered_count;
            float processing_time_ms;
            std::string algorithm_version;
            
            nlohmann::json to_json() const {
                return nlohmann::json{
                    {"total_count", total_count},
                    {"filtered_count", filtered_count},
                    {"processing_time_ms", processing_time_ms},
                    {"algorithm_version", algorithm_version}
                };
            }
        };
        
        ResponseMetadata metadata;
        
        // Error handling
        std::string error;
        
        // Constructor
        CustomResponseModel() {
            created = std::time(nullptr);
        }
        
        // IModel interface
        bool validate() const override;
        void from_json(const nlohmann::json& j) override;
        nlohmann::json to_json() const override;
        
        // Helper methods
        bool hasError() const { return !error.empty(); }
        void setError(const std::string& errorMsg) { error = errorMsg; }
        void addResult(const ResultItem& item) { results.push_back(item); }
    };
    
} // namespace kolosal
```

**File**: `src/models/custom_response_model.cpp`

```cpp
#include "kolosal/models/custom_response_model.hpp"

namespace kolosal {
    
    bool CustomResponseModel::validate() const {
        // Basic validation
        if (id.empty() || model.empty()) {
            return false;
        }
        
        if (created <= 0) {
            return false;
        }
        
        // Validate results
        for (const auto& result : results) {
            if (result.item_id.empty() || result.content.empty()) {
                return false;
            }
            if (result.score < 0.0f || result.score > 1.0f) {
                return false;
            }
        }
        
        return true;
    }
    
    void CustomResponseModel::from_json(const nlohmann::json& j) {
        if (j.contains("id")) id = j["id"].get<std::string>();
        if (j.contains("object")) object = j["object"].get<std::string>();
        if (j.contains("created")) created = j["created"].get<int64_t>();
        if (j.contains("model")) model = j["model"].get<std::string>();
        if (j.contains("error")) error = j["error"].get<std::string>();
        
        // Parse results array
        if (j.contains("results") && j["results"].is_array()) {
            results.clear();
            for (const auto& item : j["results"]) {
                ResultItem result;
                result.item_id = item["item_id"].get<std::string>();
                result.content = item["content"].get<std::string>();
                result.score = item["score"].get<float>();
                
                if (item.contains("categories") && item["categories"].is_array()) {
                    for (const auto& cat : item["categories"]) {
                        result.categories.push_back(cat.get<std::string>());
                    }
                }
                
                results.push_back(result);
            }
        }
        
        // Parse metadata
        if (j.contains("metadata")) {
            const auto& meta = j["metadata"];
            metadata.total_count = meta.value("total_count", 0);
            metadata.filtered_count = meta.value("filtered_count", 0);
            metadata.processing_time_ms = meta.value("processing_time_ms", 0.0f);
            metadata.algorithm_version = meta.value("algorithm_version", "");
        }
    }
    
    nlohmann::json CustomResponseModel::to_json() const {
        nlohmann::json j = {
            {"id", id},
            {"object", object},
            {"created", created},
            {"model", model}
        };
        
        // Add error if present
        if (!error.empty()) {
            j["error"] = error;
        }
        
        // Add results
        nlohmann::json resultsArray = nlohmann::json::array();
        for (const auto& result : results) {
            resultsArray.push_back(result.to_json());
        }
        j["results"] = resultsArray;
        
        // Add metadata
        j["metadata"] = metadata.to_json();
        
        return j;
    }
    
} // namespace kolosal
```

### 4. Update Build Configuration

Add the new model files to `CMakeLists.txt`:

```cmake
set(SOURCES
    # ... existing sources ...
    src/models/custom_request_model.cpp
    src/models/custom_response_model.cpp
    # ... other sources ...
)
```

## Advanced Model Patterns

### 1. Polymorphic Models

For models that need different implementations:

```cpp
// Base model
class BaseRequestModel : public IModel {
public:
    std::string type;
    virtual bool validate() const override = 0;
    virtual void from_json(const nlohmann::json& j) override = 0;
    virtual nlohmann::json to_json() const override = 0;
};

// Derived models
class SearchRequestModel : public BaseRequestModel {
public:
    std::string query;
    int max_results = 10;
    
    bool validate() const override {
        return !query.empty() && max_results > 0;
    }
    
    void from_json(const nlohmann::json& j) override {
        type = "search";
        query = j["query"].get<std::string>();
        if (j.contains("max_results")) {
            max_results = j["max_results"].get<int>();
        }
    }
    
    nlohmann::json to_json() const override {
        return nlohmann::json{
            {"type", type},
            {"query", query},
            {"max_results", max_results}
        };
    }
};

class ClassifyRequestModel : public BaseRequestModel {
public:
    std::string text;
    std::vector<std::string> categories;
    
    bool validate() const override {
        return !text.empty() && !categories.empty();
    }
    
    void from_json(const nlohmann::json& j) override {
        type = "classify";
        text = j["text"].get<std::string>();
        categories = j["categories"].get<std::vector<std::string>>();
    }
    
    nlohmann::json to_json() const override {
        return nlohmann::json{
            {"type", type},
            {"text", text},
            {"categories", categories}
        };
    }
};
```

### 2. Model Factory Pattern

For creating models based on request type:

```cpp
class ModelFactory {
public:
    static std::unique_ptr<BaseRequestModel> createRequest(const nlohmann::json& j) {
        if (!j.contains("type")) {
            throw std::invalid_argument("Missing request type");
        }
        
        std::string type = j["type"].get<std::string>();
        
        if (type == "search") {
            auto model = std::make_unique<SearchRequestModel>();
            model->from_json(j);
            return model;
        } else if (type == "classify") {
            auto model = std::make_unique<ClassifyRequestModel>();
            model->from_json(j);
            return model;
        } else {
            throw std::invalid_argument("Unknown request type: " + type);
        }
    }
};
```

### 3. Validation with Custom Validators

```cpp
class ValidatorBase {
public:
    virtual bool validate(const std::string& value) const = 0;
    virtual std::string getErrorMessage() const = 0;
    virtual ~ValidatorBase() = default;
};

class EmailValidator : public ValidatorBase {
public:
    bool validate(const std::string& value) const override {
        // Simple email validation
        return value.find('@') != std::string::npos && 
               value.find('.') != std::string::npos;
    }
    
    std::string getErrorMessage() const override {
        return "Invalid email format";
    }
};

class URLValidator : public ValidatorBase {
public:
    bool validate(const std::string& value) const override {
        return value.substr(0, 7) == "http://" || 
               value.substr(0, 8) == "https://";
    }
    
    std::string getErrorMessage() const override {
        return "Invalid URL format";
    }
};

// Usage in model
class ContactModel : public IModel {
public:
    std::string email;
    std::string website;
    
    bool validate() const override {
        EmailValidator emailValidator;
        URLValidator urlValidator;
        
        if (!emailValidator.validate(email)) {
            return false;
        }
        
        if (!website.empty() && !urlValidator.validate(website)) {
            return false;
        }
        
        return true;
    }
    
    // ... rest of implementation
};
```

### 4. Model with Conditional Fields

```cpp
class ConditionalModel : public IModel {
public:
    std::string mode;  // "simple" or "advanced"
    
    // Simple mode fields
    std::optional<std::string> simple_query;
    
    // Advanced mode fields
    std::optional<std::string> advanced_query;
    std::optional<std::vector<std::string>> filters;
    std::optional<int> boost_factor;
    
    bool validate() const override {
        if (mode != "simple" && mode != "advanced") {
            return false;
        }
        
        if (mode == "simple") {
            return simple_query.has_value() && !simple_query.value().empty();
        } else {
            return advanced_query.has_value() && !advanced_query.value().empty();
        }
    }
    
    void from_json(const nlohmann::json& j) override {
        mode = j["mode"].get<std::string>();
        
        if (mode == "simple") {
            if (j.contains("query")) {
                simple_query = j["query"].get<std::string>();
            }
        } else if (mode == "advanced") {
            if (j.contains("query")) {
                advanced_query = j["query"].get<std::string>();
            }
            if (j.contains("filters")) {
                filters = j["filters"].get<std::vector<std::string>>();
            }
            if (j.contains("boost_factor")) {
                boost_factor = j["boost_factor"].get<int>();
            }
        }
    }
    
    nlohmann::json to_json() const override {
        nlohmann::json j = {{"mode", mode}};
        
        if (mode == "simple" && simple_query.has_value()) {
            j["query"] = simple_query.value();
        } else if (mode == "advanced") {
            if (advanced_query.has_value()) {
                j["query"] = advanced_query.value();
            }
            if (filters.has_value()) {
                j["filters"] = filters.value();
            }
            if (boost_factor.has_value()) {
                j["boost_factor"] = boost_factor.value();
            }
        }
        
        return j;
    }
};
```

## Testing Models

### Unit Tests

Create tests in `tests/models/`:

```cpp
#include <gtest/gtest.h>
#include "kolosal/models/custom_request_model.hpp"

class CustomRequestModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        model = std::make_unique<CustomRequestModel>();
    }
    
    std::unique_ptr<CustomRequestModel> model;
};

TEST_F(CustomRequestModelTest, ValidateRequiredFields) {
    // Test missing required fields
    EXPECT_FALSE(model->validate());
    
    // Add required fields
    model->operation = "search";
    model->model_id = "test-model";
    EXPECT_TRUE(model->validate());
}

TEST_F(CustomRequestModelTest, ValidateOptionalFields) {
    model->operation = "search";
    model->model_id = "test-model";
    
    // Valid optional fields
    model->max_results = 100;
    model->threshold = 0.8f;
    EXPECT_TRUE(model->validate());
    
    // Invalid optional fields
    model->max_results = -1;
    EXPECT_FALSE(model->validate());
}

TEST_F(CustomRequestModelTest, JsonSerialization) {
    model->operation = "search";
    model->model_id = "test-model";
    model->max_results = 50;
    model->tags = {"tag1", "tag2"};
    
    // Test to_json
    auto json = model->to_json();
    EXPECT_EQ(json["operation"], "search");
    EXPECT_EQ(json["model_id"], "test-model");
    EXPECT_EQ(json["max_results"], 50);
    
    // Test from_json
    auto model2 = std::make_unique<CustomRequestModel>();
    model2->from_json(json);
    EXPECT_EQ(model2->operation, "search");
    EXPECT_EQ(model2->model_id, "test-model");
    EXPECT_EQ(model2->max_results.value(), 50);
}

TEST_F(CustomRequestModelTest, JsonParsingErrors) {
    nlohmann::json invalid_json = {
        {"operation", "search"}
        // Missing model_id
    };
    
    EXPECT_THROW(model->from_json(invalid_json), std::invalid_argument);
}
```

### Integration Tests

```cpp
// Test model usage in route
TEST(RouteIntegrationTest, CustomModelHandling) {
    // Create test request
    nlohmann::json request = {
        {"operation", "search"},
        {"model_id", "test-model"},
        {"max_results", 10},
        {"tags", {"test", "example"}}
    };
    
    // Parse with model
    CustomRequestModel model;
    EXPECT_NO_THROW(model.from_json(request));
    EXPECT_TRUE(model.validate());
    
    // Verify parsed data
    EXPECT_EQ(model.operation, "search");
    EXPECT_EQ(model.getMaxResults(), 10);
    EXPECT_EQ(model.tags.size(), 2);
}
```

## Best Practices

### 1. Validation
- Always validate required fields
- Check field ranges and formats
- Provide clear error messages
- Validate nested objects recursively

### 2. JSON Handling
- Use try-catch for JSON parsing
- Handle missing optional fields gracefully
- Validate JSON structure before parsing
- Provide meaningful parsing error messages

### 3. Optional Fields
- Use `std::optional` for optional fields
- Provide sensible defaults
- Check presence before accessing values
- Document default behavior

### 4. Performance
- Minimize object copying
- Use move semantics where appropriate
- Cache validation results if expensive
- Consider lazy validation for complex models

### 5. Documentation
- Document all fields and their purposes
- Provide usage examples
- Document validation rules
- Keep API documentation synchronized

### 6. Backward Compatibility
- Don't remove existing fields
- Add new fields as optional
- Maintain default values
- Version your models when necessary

This comprehensive guide should help you create robust, well-structured models for the Kolosal Server that handle data validation, JSON serialization, and integration with the route system effectively.

# Add Engine Request DTO Model

This document demonstrates how to use the new `AddEngineRequest` DTO model in the `add_engine_route.cpp`.

## Overview

The `AddEngineRequest` model provides a structured way to handle JSON request parsing for the add engine endpoint, replacing manual JSON parsing with a type-safe, validated model approach.

## Usage Example

### Before (Manual JSON Parsing)
```cpp
// Old approach - manual JSON parsing
auto j = json::parse(body);
std::string engineId = j["engine_id"];
std::string modelPath = j["model_path"];
bool loadImmediately = j.value("load_immediately", true);

if (j.contains("loading_parameters")) {
    auto loadParamsJson = j["loading_parameters"];
    loadParams.n_ctx = loadParamsJson.value("n_ctx", 4096);
    // ... more manual parsing
}
```

### After (DTO Model Approach)
```cpp
// New approach - structured DTO model
auto j = json::parse(body);
AddEngineRequest request;
request.from_json(j);

if (!request.validate()) {
    // Handle validation error
    return;
}

// Access validated fields
std::string engineId = request.engine_id;
std::string modelPath = request.model_path;
bool loadImmediately = request.load_immediately;

// Convert to LoadingParameters struct
LoadingParameters loadParams;
loadParams.n_ctx = request.loading_parameters.n_ctx;
loadParams.n_keep = request.loading_parameters.n_keep;
// ... etc
```

## JSON Schema

The `AddEngineRequest` model expects JSON in the following format:

```json
{
  "engine_id": "my-engine",
  "model_path": "/path/to/model.gguf",
  "load_immediately": true,
  "main_gpu_id": 0,
  "loading_parameters": {
    "n_ctx": 4096,
    "n_keep": 2048,
    "use_mlock": true,
    "use_mmap": true,
    "cont_batching": true,
    "warmup": false,
    "n_parallel": 1,
    "n_gpu_layers": 100,
    "n_batch": 2048,
    "n_ubatch": 512
  }
}
```

## Validation

The model includes comprehensive validation:

### Required Fields
- `engine_id` (non-empty string)
- `model_path` (non-empty string)

### Optional Fields with Defaults
- `load_immediately` (boolean, default: true)
- `main_gpu_id` (integer, default: 0, range: -1 to 15)
- `loading_parameters` (object with nested validation)

### Loading Parameters Validation
- `n_ctx`: 1 to 1,000,000
- `n_keep`: 0 to n_ctx
- `n_batch`: 1 to 8,192
- `n_ubatch`: 1 to n_batch
- `n_parallel`: 1 to 16
- `n_gpu_layers`: 0 to 1,000

## Benefits

1. **Type Safety**: Compile-time checking of field types
2. **Validation**: Built-in parameter validation with detailed error messages
3. **Maintainability**: Centralized request structure definition
4. **Consistency**: Follows the same pattern as `CompletionRequest` model
5. **Error Handling**: Structured exception handling for validation errors
6. **Documentation**: Self-documenting code with clear field definitions

## Files Created

- `include/kolosal/models/add_engine_request_model.hpp` - Request DTO model
- `include/kolosal/models/add_engine_response_model.hpp` - Response DTO model (optional)

## Integration

The model is integrated into `add_engine_route.cpp` with:
- JSON parsing and validation
- Type-safe field access
- Structured error handling
- Response generation using the validated model data

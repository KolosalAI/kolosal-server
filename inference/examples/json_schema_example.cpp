/**
 * @file json_schema_example.cpp
 * @brief Example demonstrating JSON Schema-based generation using the Inference Engine
 * 
 * This example shows how to use JSON Schema to constrain model outputs
 * to specific structures, automatically converting schemas to grammars.
 */

#include <iostream>
#include <string>
#include "inference.h"
#include "json.hpp"

using json = nlohmann::json;

int main() {
    // Create inference engine
    InferenceEngine engine;
    
    // Load model parameters
    LoadingParameters loadParams;
    loadParams.n_ctx = 2048;
    loadParams.n_gpu_layers = -1; // Use all GPU layers if available
    
    // Load a model (replace with your model path)
    const char* modelPath = "path/to/your/model.gguf";
    if (!engine.loadModel(modelPath, loadParams)) {
        std::cerr << "Failed to load model" << std::endl;
        return 1;
    }
    
    // Example 1: Simple Person Schema
    {
        std::cout << "\n=== Example 1: Person Schema ===" << std::endl;
        
        // Define a JSON schema for a person object
        json personSchema = {
            {"type", "object"},
            {"properties", {
                {"name", {
                    {"type", "string"},
                    {"description", "The person's full name"}
                }},
                {"age", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 150}
                }},
                {"email", {
                    {"type", "string"},
                    {"format", "email"}
                }},
                {"isEmployed", {
                    {"type", "boolean"}
                }}
            }},
            {"required", {"name", "age", "email", "isEmployed"}}
        };
        
        CompletionParameters params;
        params.prompt = "Generate information for a software developer named John:";
        params.jsonSchema = personSchema.dump();
        params.maxNewTokens = 200;
        params.temperature = 0.7f;
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Generated person data: " << result.text << std::endl;
            
            // Parse and validate the result
            try {
                json parsedResult = json::parse(result.text);
                std::cout << "Parsed successfully!" << std::endl;
                std::cout << "Name: " << parsedResult["name"] << std::endl;
                std::cout << "Age: " << parsedResult["age"] << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse result: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Error: " << engine.getJobError(jobId) << std::endl;
        }
    }
    
    // Example 2: Array of Items Schema
    {
        std::cout << "\n=== Example 2: Shopping List Schema ===" << std::endl;
        
        // Define a schema for a shopping list
        json shoppingListSchema = {
            {"type", "object"},
            {"properties", {
                {"store", {
                    {"type", "string"},
                    {"description", "Name of the store"}
                }},
                {"date", {
                    {"type", "string"},
                    {"format", "date"}
                }},
                {"items", {
                    {"type", "array"},
                    {"items", {
                        {"type", "object"},
                        {"properties", {
                            {"name", {"type", "string"}},
                            {"quantity", {
                                {"type", "integer"},
                                {"minimum", 1}
                            }},
                            {"price", {
                                {"type", "number"},
                                {"minimum", 0}
                            }}
                        }},
                        {"required", {"name", "quantity", "price"}}
                    }},
                    {"minItems", 1},
                    {"maxItems", 10}
                }}
            }},
            {"required", {"store", "date", "items"}}
        };
        
        CompletionParameters params;
        params.prompt = "Create a grocery shopping list for making pasta dinner:";
        params.jsonSchema = shoppingListSchema.dump();
        params.maxNewTokens = 400;
        params.temperature = 0.8f;
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Generated shopping list:\n" << result.text << std::endl;
        }
    }
    
    // Example 3: Enum and Nested Objects
    {
        std::cout << "\n=== Example 3: Product Review Schema ===" << std::endl;
        
        // Define a schema for a product review
        json reviewSchema = {
            {"type", "object"},
            {"properties", {
                {"productName", {"type", "string"}},
                {"rating", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 5}
                }},
                {"sentiment", {
                    {"type", "string"},
                    {"enum", {"positive", "neutral", "negative"}}
                }},
                {"review", {
                    {"type", "object"},
                    {"properties", {
                        {"title", {"type", "string"}},
                        {"body", {
                            {"type", "string"},
                            {"minLength", 10},
                            {"maxLength", 500}
                        }},
                        {"pros", {
                            {"type", "array"},
                            {"items", {"type", "string"}},
                            {"minItems", 1},
                            {"maxItems", 5}
                        }},
                        {"cons", {
                            {"type", "array"},
                            {"items", {"type", "string"}},
                            {"maxItems", 5}
                        }}
                    }},
                    {"required", {"title", "body", "pros"}}
                }}
            }},
            {"required", {"productName", "rating", "sentiment", "review"}}
        };
        
        CompletionParameters params;
        params.prompt = "Write a review for a wireless gaming mouse:";
        params.jsonSchema = reviewSchema.dump();
        params.maxNewTokens = 600;
        params.temperature = 0.7f;
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Generated review:\n" << result.text << std::endl;
        }
    }
    
    // Example 4: Chat Completion with JSON Schema
    {
        std::cout << "\n=== Example 4: Structured Chat Response ===" << std::endl;
        
        // Define a schema for a structured assistant response
        json responseSchema = {
            {"type", "object"},
            {"properties", {
                {"answer", {
                    {"type", "string"},
                    {"description", "The main answer to the question"}
                }},
                {"confidence", {
                    {"type", "string"},
                    {"enum", {"high", "medium", "low"}}
                }},
                {"sources", {
                    {"type", "array"},
                    {"items", {"type", "string"}},
                    {"description", "Sources or reasoning for the answer"}
                }},
                {"followUp", {
                    {"type", "string"},
                    {"description", "A follow-up question or suggestion"}
                }}
            }},
            {"required", {"answer", "confidence"}}
        };
        
        ChatCompletionParameters chatParams;
        chatParams.messages.push_back(Message("system", 
            "You are a helpful assistant that provides structured responses."));
        chatParams.messages.push_back(Message("user", 
            "What are the benefits of regular exercise?"));
        chatParams.jsonSchema = responseSchema.dump();
        chatParams.maxNewTokens = 400;
        chatParams.temperature = 0.6f;
        
        int jobId = engine.submitChatCompletionsJob(chatParams);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Structured response:\n" << result.text << std::endl;
            
            // Parse and display structured data
            try {
                json parsed = json::parse(result.text);
                std::cout << "\nConfidence: " << parsed["confidence"] << std::endl;
                if (parsed.contains("sources")) {
                    std::cout << "Sources: " << parsed["sources"].size() << " provided" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse response: " << e.what() << std::endl;
            }
        }
    }
    
    // Example 5: Complex Nested Schema
    {
        std::cout << "\n=== Example 5: API Response Schema ===" << std::endl;
        
        // Define a schema for an API response
        json apiResponseSchema = {
            {"type", "object"},
            {"properties", {
                {"status", {
                    {"type", "string"},
                    {"enum", {"success", "error", "pending"}}
                }},
                {"data", {
                    {"type", "object"},
                    {"properties", {
                        {"user", {
                            {"type", "object"},
                            {"properties", {
                                {"id", {"type", "integer"}},
                                {"username", {"type", "string"}},
                                {"profile", {
                                    {"type", "object"},
                                    {"properties", {
                                        {"bio", {"type", "string"}},
                                        {"avatar", {"type", "string"}},
                                        {"verified", {"type", "boolean"}}
                                    }}
                                }}
                            }},
                            {"required", {"id", "username"}}
                        }},
                        {"timestamp", {
                            {"type", "string"},
                            {"format", "date-time"}
                        }}
                    }}
                }},
                {"error", {
                    {"type", {"object", "null"}},
                    {"properties", {
                        {"code", {"type", "integer"}},
                        {"message", {"type", "string"}}
                    }}
                }}
            }},
            {"required", {"status", "data"}}
        };
        
        CompletionParameters params;
        params.prompt = "Generate a successful API response for a user profile request:";
        params.jsonSchema = apiResponseSchema.dump();
        params.maxNewTokens = 400;
        params.temperature = 0.5f;
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Generated API response:\n" << result.text << std::endl;
        }
    }
    
    // Unload model
    engine.unloadModel();
    
    std::cout << "\n=== JSON Schema Examples Complete ===" << std::endl;
    
    return 0;
}
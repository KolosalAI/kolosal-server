/**
 * @file grammar_example.cpp
 * @brief Example demonstrating grammar-constrained generation using the Inference Engine
 * 
 * This example shows how to use GBNF (Grammar-Based Natural Format) grammars
 * to constrain the output of language models to specific formats.
 */

#include <iostream>
#include <string>
#include "inference.h"

// Example GBNF grammars
namespace grammars {
    // JSON grammar - constrains output to valid JSON
    const std::string json = R"(
root   ::= object
value  ::= object | array | string | number | ("true" | "false" | "null") ws

object ::=
  "{" ws (
            string ":" ws value
    ("," ws string ":" ws value)*
  )? "}" ws

array  ::=
  "[" ws (
            value
    ("," ws value)*
  )? "]" ws

string ::=
  "\"" (
    [^"\\] |
    "\\" (["\\/bfnrt] | "u" [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F])
  )* "\"" ws

number ::= ("-"? ([0-9] | [1-9] [0-9]*)) ("." [0-9]+)? ([eE] [-+]? [0-9]+)? ws

ws ::= ([ \t\n] ws)?
)";

    // Simple list grammar - constrains output to a numbered list
    const std::string list = R"(
root ::= item+
item ::= number "." " " text "\n"
number ::= [0-9]+
text ::= [^\n]+
)";

    // Yes/No grammar - constrains output to only "Yes" or "No"
    const std::string yes_no = R"(
root ::= ("Yes" | "No")
)";

    // Python function grammar - constrains output to valid Python function
    const std::string python_function = R"(
root ::= "def " identifier "(" params? "):" "\n" body
identifier ::= [a-zA-Z_] [a-zA-Z0-9_]*
params ::= param ("," " " param)*
param ::= identifier
body ::= "    " statement ("\n" "    " statement)*
statement ::= [^\n]+
)";
}

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
    
    // Example 1: Generate JSON with grammar constraint
    {
        std::cout << "\n=== Example 1: JSON Generation ===" << std::endl;
        
        CompletionParameters params;
        params.prompt = "Generate a JSON object describing a person with name, age, and city:";
        params.grammar = grammars::json;
        params.maxNewTokens = 200;
        params.temperature = 0.7f;
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Generated JSON: " << result.text << std::endl;
        } else {
            std::cerr << "Error: " << engine.getJobError(jobId) << std::endl;
        }
    }
    
    // Example 2: Generate a numbered list
    {
        std::cout << "\n=== Example 2: List Generation ===" << std::endl;
        
        CompletionParameters params;
        params.prompt = "List 5 benefits of regular exercise:";
        params.grammar = grammars::list;
        params.maxNewTokens = 300;
        params.temperature = 0.8f;
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Generated list:\n" << result.text << std::endl;
        }
    }
    
    // Example 3: Yes/No answer
    {
        std::cout << "\n=== Example 3: Yes/No Answer ===" << std::endl;
        
        CompletionParameters params;
        params.prompt = "Is the Earth round? Answer with only Yes or No:";
        params.grammar = grammars::yes_no;
        params.maxNewTokens = 10;
        params.temperature = 0.1f; // Low temperature for deterministic output
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Answer: " << result.text << std::endl;
        }
    }
    
    // Example 4: Chat completion with grammar
    {
        std::cout << "\n=== Example 4: Chat with JSON Response ===" << std::endl;
        
        ChatCompletionParameters chatParams;
        chatParams.messages.push_back(Message("system", "You are a helpful assistant that responds in JSON format."));
        chatParams.messages.push_back(Message("user", "What is the weather like today?"));
        chatParams.grammar = grammars::json;
        chatParams.maxNewTokens = 200;
        chatParams.temperature = 0.7f;
        
        int jobId = engine.submitChatCompletionsJob(chatParams);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Chat response: " << result.text << std::endl;
        }
    }
    
    // Example 5: Generate Python function
    {
        std::cout << "\n=== Example 5: Python Function Generation ===" << std::endl;
        
        CompletionParameters params;
        params.prompt = "Write a Python function to calculate factorial:";
        params.grammar = grammars::python_function;
        params.maxNewTokens = 200;
        params.temperature = 0.5f;
        
        int jobId = engine.submitCompletionsJob(params);
        engine.waitForJob(jobId);
        
        if (!engine.hasJobError(jobId)) {
            CompletionResult result = engine.getJobResult(jobId);
            std::cout << "Generated function:\n" << result.text << std::endl;
        }
    }
    
    // Unload model
    engine.unloadModel();
    
    return 0;
}
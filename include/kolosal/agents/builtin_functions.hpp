// File: include/kolosal/agents/builtin_functions.hpp
#pragma once

#include "../export.hpp"
#include "agent_interfaces.hpp"
#include "agent_data.hpp"
#include "yaml_config.hpp"
#include <memory>

namespace kolosal::agents {

// Forward declarations
class FunctionManager;

/**
 * @brief Basic arithmetic function
 */
class KOLOSAL_SERVER_API AddFunction : public AgentFunction {
public:
    std::string get_name() const override { return "add"; }
    std::string get_description() const override { return "Add two numbers"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Echo function with optional transformations
 */
class KOLOSAL_SERVER_API EchoFunction : public AgentFunction {
public:
    std::string get_name() const override { return "echo"; }
    std::string get_description() const override { return "Echo a message with optional processing"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Delay/sleep function
 */
class KOLOSAL_SERVER_API DelayFunction : public AgentFunction {
public:
    std::string get_name() const override { return "delay"; }
    std::string get_description() const override { return "Wait for specified milliseconds"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Text analysis function (supports both text_analysis and text_processing names)
 */
class KOLOSAL_SERVER_API TextAnalysisFunction : public AgentFunction {
public:
    std::string get_name() const override { return "text_analysis"; }
    std::string get_description() const override { return "Analyze text for word count, character count, and sentiment"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Text processing function (alias for TextAnalysisFunction to match YAML config)
 */
class KOLOSAL_SERVER_API TextProcessingFunction : public AgentFunction {
public:
    std::string get_name() const override { return "text_processing"; }
    std::string get_description() const override { return "Process and analyze text content"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Data transformation function
 */
class KOLOSAL_SERVER_API DataTransformFunction : public AgentFunction {
public:
    std::string get_name() const override { return "data_transform"; }
    std::string get_description() const override { return "Transform data arrays with various operations"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Data analysis function
 */
class KOLOSAL_SERVER_API DataAnalysisFunction : public AgentFunction {
public:
    std::string get_name() const override { return "data_analysis"; }
    std::string get_description() const override { return "Analyze structured data and extract insights"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Inference function that bridges to Kolosal inference engines
 */
class KOLOSAL_SERVER_API InferenceFunction : public AgentFunction {
private:
    std::string engine_id;
    
public:
    InferenceFunction(const std::string& engine = "default");
    
    std::string get_name() const override { return "inference"; }
    std::string get_description() const override { return "Run inference using the specified engine"; }
    std::string get_type() const override { return "inference"; }
    FunctionResult execute(const AgentData& params) override;
    
    void set_engine_id(const std::string& engine) { engine_id = engine; }
    const std::string& get_engine_id() const { return engine_id; }
};

/**
 * @brief LLM-based function that uses language model for execution
 */
class KOLOSAL_SERVER_API LLMFunction : public AgentFunction {
private:
    std::string name;
    std::string description;
    std::string system_prompt;
    LLMConfig llm_config;

public:
    LLMFunction(const std::string& func_name, const std::string& func_desc, 
               const std::string& prompt, const LLMConfig& config);

    std::string get_name() const override { return name; }
    std::string get_description() const override { return description; }
    std::string get_type() const override { return "llm"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief External API function
 */
class KOLOSAL_SERVER_API ExternalAPIFunction : public AgentFunction {
private:
    std::string name;
    std::string description;
    std::string endpoint;
    std::map<std::string, std::string> headers;

public:
    ExternalAPIFunction(const std::string& func_name, const std::string& func_desc, 
                       const std::string& api_endpoint);

    std::string get_name() const override { return name; }
    std::string get_description() const override { return description; }
    std::string get_type() const override { return "external_api"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Retrieval function that bridges to Kolosal document retrieval system
 * 
 * This function allows agents to search for and retrieve relevant documents
 * from the vector database using semantic similarity search.
 */
class KOLOSAL_SERVER_API RetrievalFunction : public AgentFunction {
private:
    std::string collection_name;
    
public:
    RetrievalFunction(const std::string& collection = "documents");
    
    std::string get_name() const override { return "retrieval"; }
    std::string get_description() const override { return "Search and retrieve relevant documents from the knowledge base"; }
    std::string get_type() const override { return "retrieval"; }
    FunctionResult execute(const AgentData& params) override;
    
    void set_collection_name(const std::string& collection) { collection_name = collection; }
    const std::string& get_collection_name() const { return collection_name; }
};

/**
 * @brief Context-aware retrieval function that combines retrieval with context enhancement
 * 
 * This function not only retrieves documents but also formats them as context
 * that can be used by other agent functions, particularly LLM functions.
 */
class KOLOSAL_SERVER_API ContextRetrievalFunction : public AgentFunction {
private:
    std::string collection_name;
    
public:
    ContextRetrievalFunction(const std::string& collection = "documents");
    
    std::string get_name() const override { return "context_retrieval"; }
    std::string get_description() const override { return "Retrieve and format documents as context for enhanced agent responses"; }
    std::string get_type() const override { return "context_retrieval"; }
    FunctionResult execute(const AgentData& params) override;
    
    void set_collection_name(const std::string& collection) { collection_name = collection; }
    const std::string& get_collection_name() const { return collection_name; }
};

/**
 * @brief Tool discovery function that lists available tools and their capabilities
 */
class KOLOSAL_SERVER_API ToolDiscoveryFunction : public AgentFunction {
private:
    std::shared_ptr<FunctionManager> function_manager;
    
public:
    ToolDiscoveryFunction(std::shared_ptr<FunctionManager> fm);
    
    std::string get_name() const override { return "list_tools"; }
    std::string get_description() const override { return "List all available tools/functions and their descriptions"; }
    std::string get_type() const override { return "system"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Web search simulation function for when real web search is not available
 */
class KOLOSAL_SERVER_API WebSearchFunction : public AgentFunction {
public:
    std::string get_name() const override { return "web_search"; }
    std::string get_description() const override { return "Simulate web search functionality with mock results"; }
    std::string get_type() const override { return "simulation"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Code generation function for programming tasks
 */
class KOLOSAL_SERVER_API CodeGenerationFunction : public AgentFunction {
public:
    std::string get_name() const override { return "code_generation"; }
    std::string get_description() const override { return "Generate code snippets and programming solutions"; }
    std::string get_type() const override { return "programming"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Add documents to the retrieval system
 * 
 * This function allows agents to add new documents to the vector database
 * for future retrieval operations.
 */
class KOLOSAL_SERVER_API AddDocumentFunction : public AgentFunction {
private:
    std::string collection_name;
    
public:
    AddDocumentFunction(const std::string& collection = "documents");
    
    std::string get_name() const override { return "add_document"; }
    std::string get_description() const override { return "Add documents to the knowledge base for future retrieval"; }
    std::string get_type() const override { return "document_management"; }
    FunctionResult execute(const AgentData& params) override;
    
    void set_collection_name(const std::string& collection) { collection_name = collection; }
    const std::string& get_collection_name() const { return collection_name; }
};

/**
 * @brief Remove documents from the retrieval system
 * 
 * This function allows agents to remove documents from the vector database
 * using their document IDs.
 */
class KOLOSAL_SERVER_API RemoveDocumentFunction : public AgentFunction {
private:
    std::string collection_name;
    
public:
    RemoveDocumentFunction(const std::string& collection = "documents");
    
    std::string get_name() const override { return "remove_document"; }
    std::string get_description() const override { return "Remove documents from the knowledge base using document IDs"; }
    std::string get_type() const override { return "document_management"; }
    FunctionResult execute(const AgentData& params) override;
    
    void set_collection_name(const std::string& collection) { collection_name = collection; }
    const std::string& get_collection_name() const { return collection_name; }
};

/**
 * @brief Parse PDF documents to extract text content
 * 
 * This function allows agents to extract text content from PDF files
 * for processing or adding to the knowledge base.
 */
class KOLOSAL_SERVER_API ParsePdfFunction : public AgentFunction {
public:
    std::string get_name() const override { return "parse_pdf"; }
    std::string get_description() const override { return "Parse PDF files to extract text content"; }
    std::string get_type() const override { return "document_processing"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Parse DOCX documents to extract text content
 * 
 * This function allows agents to extract text content from Microsoft Word DOCX files
 * for processing or adding to the knowledge base.
 */
class KOLOSAL_SERVER_API ParseDocxFunction : public AgentFunction {
public:
    std::string get_name() const override { return "parse_docx"; }
    std::string get_description() const override { return "Parse DOCX files to extract text content"; }
    std::string get_type() const override { return "document_processing"; }
    FunctionResult execute(const AgentData& params) override;
};

/**
 * @brief Get embedding vector for text
 * 
 * This function allows agents to generate embedding vectors for text content
 * using the configured embedding models.
 */
class KOLOSAL_SERVER_API GetEmbeddingFunction : public AgentFunction {
private:
    std::string model_id;
    
public:
    GetEmbeddingFunction(const std::string& model = "");
    
    std::string get_name() const override { return "get_embedding"; }
    std::string get_description() const override { return "Generate embedding vectors for text content"; }
    std::string get_type() const override { return "embedding"; }
    FunctionResult execute(const AgentData& params) override;
    
    void set_model_id(const std::string& model) { model_id = model; }
    const std::string& get_model_id() const { return model_id; }
};

/**
 * @brief Test document service connection
 * 
 * This function allows agents to test the connection to the document service
 * and verify that the retrieval system is working properly.
 */
class KOLOSAL_SERVER_API TestDocumentServiceFunction : public AgentFunction {
public:
    std::string get_name() const override { return "test_document_service"; }
    std::string get_description() const override { return "Test connection to the document service and vector database"; }
    std::string get_type() const override { return "system"; }
    FunctionResult execute(const AgentData& params) override;
};

} // namespace kolosal::agents

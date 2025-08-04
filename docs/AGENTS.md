# Advanced Agent System Features

This document describes the comprehensive set of advanced features added to the Kolosal Agent System, bringing it in line with leading agent platforms like LangChain, AutoGPT, CrewAI, and others.

## 🎯 Overview of New Features

### 1. **Agent Roles & Specializations**
- **Predefined roles**: Researcher, Analyst, Writer, Critic, Executor, Coordinator
- **Specialization areas**: Data Analysis, Text Processing, Code Generation, Document Analysis, Web Research, etc.
- **Capability levels**: Basic, Intermediate, Advanced, Expert
- **Role-based function assignment**: Each role comes with appropriate default functions

### 2. **Comprehensive Tool/Function Registry**
- **Tool discovery system**: Find tools by category, tags, capabilities
- **JSON Schema support**: Standardized parameter definitions
- **Tool validation**: Automatic parameter validation
- **Cost estimation**: Built-in cost tracking for expensive operations
- **Custom tool registration**: Easy addition of new tools

### 3. **Advanced Memory Management**
- **Multi-layered memory**:
  - **Conversation Memory**: Short-term chat history with context windows
  - **Vector Memory**: Long-term semantic memory with embeddings
  - **Working Memory**: Current task context and variables
- **Semantic search**: Find relevant memories using natural language
- **Memory consolidation**: Automatic cleanup and optimization
- **Persistent storage**: Save/load memory state

### 4. **Planning & Reasoning System**
- **Goal decomposition**: Break complex goals into manageable tasks
- **Dependency management**: Handle task dependencies and prerequisites
- **Multiple planning strategies**: Sequential, parallel, priority-based, dependency-aware
- **Self-reflection**: Analyze performance and suggest improvements
- **Meta-reasoning**: Know when to ask for help or clarification

### 5. **Agent Factory & Configuration**
- **Pre-configured agent types**: Easy creation of specialized agents
- **Team creation**: Ready-made agent teams for common scenarios
- **YAML configuration**: Declarative agent setup
- **Role-based initialization**: Automatic capability and tool assignment

### 6. **Enhanced Orchestration**
- **Multi-agent coordination**: Agents can work together on complex tasks
- **Message routing**: Advanced inter-agent communication
- **Workflow management**: Define and execute multi-step processes
- **Resource allocation**: Optimize task distribution across agents

## 🚀 Quick Start

### Creating Specialized Agents

```cpp
#include "kolosal/agents/agent_factory.hpp"

// Create different types of agents
auto researcher = AgentFactory::create_researcher_agent("Alice");
auto analyst = AgentFactory::create_analyst_agent("Bob");
auto writer = AgentFactory::create_writer_agent("Charlie");

// Create a complete research team
auto team = AgentFactory::create_research_team();
```

### Using Memory System

```cpp
// Store different types of memories
agent->store_memory("Important fact about the topic", "fact");
agent->store_memory("User's question about XYZ", "conversation");

// Retrieve relevant memories
auto memories = agent->recall_memories("query about topic", 5);

// Use working memory for current context
AgentData context;
context.set("current_task", "research");
agent->set_working_context("main", context);
```

### Tool Discovery and Usage

```cpp
// Discover tools by category
ToolFilter filter;
filter.categories = {"research", "analysis"};
auto tools = agent->discover_tools(filter);

// Get tool information
auto schema = agent->get_tool_schema("web_search");

// Execute tools
AgentData params;
params.set("query", "renewable energy trends");
auto result = agent->execute_tool("web_search", params);
```

### Planning and Execution

```cpp
// Create a plan for a complex goal
auto plan = agent->create_plan(
    "Research and analyze market trends",
    "Focus on renewable energy sector"
);

// Execute the plan
bool success = agent->execute_plan(plan.id);

// Use reasoning capabilities
auto reasoning = agent->reason_about(
    "What are the key factors driving renewable energy adoption?",
    "Current market context and technological developments"
);
```

## 📊 Feature Comparison

| Feature | Previous System | New Advanced System |
|---------|----------------|-------------------|
| Agent Types | Generic only | 8+ specialized roles |
| Memory | Basic function results | Multi-layered with semantic search |
| Planning | Manual function calls | Intelligent goal decomposition |
| Tool Discovery | Static function list | Dynamic discovery with filtering |
| Reasoning | None | Built-in reasoning and reflection |
| Configuration | Code-based | YAML configuration files |
| Team Coordination | Manual | Automated orchestration |
| Performance Monitoring | Basic logging | Comprehensive analytics |

## 🛠 Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         Agent Core                          │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │ Role System │  │Tool Registry│  │Memory Mgmt  │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │ Planning    │  │ Reasoning   │  │Orchestration│          │
│  │ System      │  │ System      │  │   System    │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
├─────────────────────────────────────────────────────────────┤
│              Legacy Function & Event Systems                │
└─────────────────────────────────────────────────────────────┘
```

## 📝 Configuration Examples

### Agent Configuration (YAML)

```yaml
agents:
  - name: "ResearchSpecialist"
    role: "researcher"
    specializations:
      - "web_research"
      - "document_analysis"
    capabilities:
      - "web_search"
      - "fact_checking"
    memory_config:
      conversation_limit: 200
      vector_memory_enabled: true
    tools:
      - "web_search"
      - "parse_pdf"
      - "context_retrieval"
```

### Workflow Configuration

```yaml
workflows:
  research_workflow:
    goal: "Comprehensive market research"
    agents: ["Coordinator", "Researcher", "Analyst"]
    steps:
      - name: "planning"
        agent: "Coordinator"
        action: "create_plan"
      - name: "research"
        agent: "Researcher"
        action: "gather_data"
        depends_on: ["planning"]
      - name: "analysis"
        agent: "Analyst"
        action: "analyze_data"
        depends_on: ["research"]
```

## 🎯 Use Cases

### 1. Research & Analysis
- **Automated research workflows**: Web scraping, document analysis, fact-checking
- **Multi-source data gathering**: Coordinate multiple research agents
- **Intelligent summarization**: Extract key insights from large datasets

### 2. Content Creation
- **Collaborative writing**: Research, writing, and review by specialized agents
- **Quality assurance**: Automated content review and improvement suggestions
- **Multi-format output**: Generate content in various formats and styles

### 3. Data Processing
- **Complex data pipelines**: Multi-step data transformation and analysis
- **Pattern recognition**: Identify trends and anomalies in large datasets
- **Automated reporting**: Generate insights and recommendations

### 4. Code Generation & Analysis
- **Intelligent code generation**: Context-aware programming assistance
- **Code review and optimization**: Automated code quality assessment
- **Documentation generation**: Create comprehensive technical documentation

## 🔧 Advanced Features

### Custom Tool Development

```cpp
class CustomAnalysisTool : public BaseTool {
public:
    CustomAnalysisTool() : BaseTool("custom_analysis", "Custom data analysis", "analysis") {
        add_parameter(ToolParameter("data", "string", "Input data", true));
        add_parameter(ToolParameter("method", "string", "Analysis method", false));
        add_tag("custom");
        add_tag("analysis");
    }
    
    FunctionResult execute(const AgentData& params, const ToolContext& context) override {
        // Implementation here
        return FunctionResult(true);
    }
};

// Register the tool
agent->register_custom_tool(std::make_unique<CustomAnalysisTool>());
```

### Memory Persistence

```cpp
// Save agent memory to file
agent->get_memory_manager()->save_to_file("agent_memory.dat");

// Load memory from file
agent->get_memory_manager()->load_from_file("agent_memory.dat");
```

### Advanced Planning

```cpp
// Create complex multi-step plans
auto plan = planning_system->decompose_goal(
    "Create a comprehensive market analysis report",
    "Include competitor analysis, market trends, and recommendations",
    PlanningStrategy::DEPENDENCY_AWARE
);

// Monitor plan execution
auto stats = planning_system->get_statistics();
std::cout << "Plans completed: " << stats.completed_plans << std::endl;
std::cout << "Success rate: " << stats.success_rate << std::endl;
```

## 📈 Performance & Monitoring

### Built-in Analytics
- **Execution metrics**: Track function/tool execution times
- **Memory usage**: Monitor memory consumption and optimization
- **Success rates**: Track task completion and failure rates
- **Agent utilization**: Monitor workload distribution

### Health Monitoring
- **Automatic cleanup**: Remove old, unused memories
- **Performance optimization**: Suggest improvements based on usage patterns
- **Resource management**: Balance workload across agents

## What is an Agent?

An **Agent** in Kolosal is an autonomous, modular component capable of executing functions, handling messages, and collaborating with other agents. Agents can be configured with specific capabilities and functions, and communicate via a sophisticated message routing system with support for workflow orchestration and RAG (Retrieval-Augmented Generation) capabilities.

## Key Components

### Core Architecture

- **AgentCore**: The main class representing an individual agent. Handles lifecycle (start/stop), function execution, message handling, and capability management. Each agent has a unique ID and supports concurrent job execution with thread-safe operations.
- **YAMLConfigurableAgentManager**: The system-wide agent manager that loads agent and function configurations from YAML files, manages agent lifecycle, supports hot-reloading, and provides centralized agent orchestration.
- **AgentOrchestrator**: Advanced workflow orchestration system that coordinates multi-agent collaboration, manages complex workflows, and handles inter-agent dependencies with support for multiple collaboration patterns.
- **WorkflowEngine**: Advanced workflow execution engine that supports sequential, parallel, pipeline, consensus, and conditional workflows with sophisticated error handling and retry mechanisms.
- **FunctionManager**: Registers and manages functions that agents can execute. Supports multiple function types:
  - **Builtin functions**: Native server functions (inference, text_processing, data_analysis, retrieval, context_retrieval, add_document, remove_document, parse_pdf, parse_docx, get_embedding)
  - **LLM functions**: Functions that leverage language models for processing
  - **External API functions**: Integration with external services (web_search, etc.)
  - **Custom functions**: User-defined function implementations
  - **Retrieval functions**: Vector search and document retrieval capabilities with semantic similarity matching
- **JobManager**: Handles asynchronous job execution with priority queuing, job status tracking, and result management.
- **EventSystem**: Comprehensive event handling system that emits and processes agent-related events (message received, function executed, job completed, etc.).
- **MessageRouter**: Advanced message routing system supporting direct messaging, broadcasting, priority handling, and correlation tracking.
- **DocumentService**: Manages document indexing, embedding generation, and vector storage for retrieval-augmented generation (RAG) capabilities.
- **ConfigurableAgentFactory**: Factory for creating agents from YAML configurations with support for various agent types and function registrations.

## Agent Lifecycle

1. **Configuration Loading**: Agents are created from YAML configuration files or programmatically via API calls, with support for multiple agent types and specialized roles.
2. **Initialization**: Each agent receives a unique ID, name, type, and role configuration. The system supports the following predefined agent types:
   - **Research Agents** (`research`) - Information gathering, web search, document retrieval, and analysis
   - **Development Agents** (`development`) - Code generation, review, debugging, and optimization
   - **Analytics Agents** (`analytics`) - Data analysis, statistical processing, and visualization
   - **Creative Agents** (`creative`) - Content creation, writing, and copywriting
   - **Management Agents** (`management`) - Project coordination, task management, and progress tracking
   - **QA Agents** (`quality_assurance`) - Quality assurance, testing, validation, and process improvement
   - **Document Management** (`document_management`) - Document processing, knowledge base management, and content organization
   - **General Purpose** (`general`) - Flexible agents for various tasks and testing purposes
3. **Capability Assignment**: Agents are assigned specific capabilities based on their type and configuration (e.g., text_processing, code_generation, data_analysis, web_search, document_retrieval, document_management, document_parsing).
4. **Function Registration**: Functions are registered to agents based on their type and capabilities. Functions can be:
   - **Built-in**: Native server functions (inference, text_processing, data_analysis, retrieval, context_retrieval, add_document, remove_document, parse_pdf, parse_docx, get_embedding, test_document_service)
   - **LLM-powered**: Functions that use language models with customizable prompts and parameters
   - **External API**: Integration with external services (web_search, etc.)
   - **Custom**: User-defined implementations
   - **Document Processing**: PDF and DOCX parsing, document management, and embedding generation
   - **Retrieval**: Vector search and document retrieval for RAG capabilities
5. **Message System Setup**: Agents are connected to the message routing system for inter-agent communication with support for direct messages, broadcasts, and workflow coordination.
6. **Execution Management**: Agents can execute functions both synchronously and asynchronously through a sophisticated job management system with priority queuing.
7. **Event Handling**: Agents emit and respond to system events for coordination, monitoring, and workflow management.
8. **Health Monitoring**: Continuous health checks, heartbeat monitoring, and automatic recovery mechanisms.

## Example Agent Configuration (YAML)

The system supports comprehensive YAML configuration for agents, functions, and system settings:

```yaml
# System-wide configuration
system:
  worker_threads: 8
  health_check_interval_seconds: 30
  log_level: debug

# Database configuration for RAG
database:
  qdrant:
    enabled: true
    host: localhost
    port: 6333
    collection_name: documents
    default_embedding_model: text-embedding-3-small
    timeout: 60
    api_key: ""
    max_connections: 20
    connection_timeout: 10

# Model configurations
models:
  - id: qwen3-0.6b
    path: downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf
    type: llm
    load_immediately: true
    load_params:
      n_ctx: 2048
      n_gpu_layers: 100
      n_batch: 512
  - id: text-embedding-3-small
    path: downloads/Qwen3-Embedding-0.6B-Q8_0.gguf
    type: embedding
    load_immediately: true
    load_params:
      n_ctx: 2048
      n_gpu_layers: 100

# Function definitions
functions:
  - name: inference
    type: builtin
    description: Generate text using the inference engine
    async_capable: true
    timeout_ms: 60000
    
  - name: retrieval
    type: builtin
    description: Search and retrieve relevant documents from knowledge base
    async_capable: true
    timeout_ms: 30000
    
  - name: context_retrieval
    type: builtin
    description: Retrieve and format documents as context for enhanced responses
    async_capable: true
    timeout_ms: 30000
    
  - name: add_document
    type: builtin
    description: Add documents to the knowledge base for future retrieval
    async_capable: true
    timeout_ms: 60000
    
  - name: remove_document
    type: builtin
    description: Remove documents from the knowledge base using document IDs
    async_capable: true
    timeout_ms: 30000
    
  - name: parse_pdf
    type: builtin
    description: Parse PDF files to extract text content
    async_capable: true
    timeout_ms: 120000
    
  - name: parse_docx
    type: builtin
    description: Parse DOCX files to extract text content
    async_capable: true
    timeout_ms: 60000
    
  - name: get_embedding
    type: builtin
    description: Generate embedding vectors for text content
    async_capable: true
    timeout_ms: 30000
    
  - name: test_document_service
    type: builtin
    description: Test connection to the document service and vector database
    async_capable: true
    timeout_ms: 30000
    
  - name: web_search
    type: external_api
    description: Search the web for information
    endpoint: https://api.search.com/v1/search
    async_capable: true
    timeout_ms: 60000

# Agent definitions
agents:
  - name: research_assistant
    type: research
    role: Information researcher and analyzer
    system_prompt: >
      You are a research assistant specialized in gathering, analyzing, and summarizing information.
      Your goal is to provide accurate, well-researched, and comprehensive answers to queries.
      Always cite sources when possible and indicate confidence levels in your findings.
    capabilities:
      - web_search
      - text_processing
      - data_analysis
      - information_synthesis
      - document_retrieval
      - context_retrieval
      - document_management
      - document_parsing
    functions:
      - inference
      - web_search
      - text_processing
      - data_analysis
      - retrieval
      - context_retrieval
      - add_document
      - remove_document
      - parse_pdf
      - parse_docx
      - get_embedding
      - test_document_service
    llm_config:
      api_endpoint: http://localhost:8080/v1
      instruction: You are a research assistant. Provide accurate, well-researched answers.
      temperature: 0.3
      max_tokens: 2048
      timeout_seconds: 120
      max_retries: 3
    custom_settings:
      fact_checking: enabled
      search_depth: comprehensive
    auto_start: true
    max_concurrent_jobs: 3
    heartbeat_interval_seconds: 10

  - name: document_manager
    type: document_management
    role: Document processing and knowledge base management specialist
    system_prompt: >
      You are a document management specialist who helps with organizing, processing, and managing knowledge bases.
      You can parse documents, add them to collections, retrieve relevant information, and maintain document databases.
      You ensure data quality and provide efficient document workflows.
    capabilities:
      - document_management
      - document_parsing
      - document_retrieval
      - knowledge_base_management
      - text_processing
    functions:
      - inference
      - add_document
      - remove_document
      - retrieval
      - context_retrieval
      - parse_pdf
      - parse_docx
      - get_embedding
      - test_document_service
      - text_processing
    llm_config:
      api_endpoint: http://localhost:8080/v1
      instruction: You are a document management specialist. Help organize and manage knowledge bases efficiently.
      temperature: 0.2
      max_tokens: 2048
      timeout_seconds: 120
      max_retries: 3
    custom_settings:
      auto_chunking: true
      batch_processing: true
      default_collection: documents
      quality_validation: enabled
    auto_start: true
    max_concurrent_jobs: 5
    heartbeat_interval_seconds: 15

  - name: knowledge_agent
    type: research
    role: Knowledge retrieval and context-aware assistant
    system_prompt: >
      You are a knowledge-aware AI assistant that uses document retrieval to provide accurate, context-rich responses.
      Always search for relevant information first, then provide comprehensive answers based on the retrieved context.
      When you don't find relevant information, clearly state this limitation.
    capabilities:
      - document_retrieval
      - context_integration
      - knowledge_synthesis
      - information_verification
    functions:
      - inference
      - retrieval
      - context_retrieval
      - text_processing
    llm_config:
      api_endpoint: http://localhost:8080/v1
      instruction: You are a knowledge-aware assistant. Always use retrieved context to provide accurate answers.
      temperature: 0.3
      max_tokens: 3072
      timeout_seconds: 90
      max_retries: 3
    custom_settings:
      context_window: large
      fact_checking: enabled
      retrieval_threshold: 0.1
    auto_start: true
    max_concurrent_jobs: 4
    heartbeat_interval_seconds: 15
```

## 📚 API Reference

For detailed API documentation, see:
- `include/kolosal/agents/agent_roles.hpp` - Role and capability definitions
- `include/kolosal/agents/tool_registry.hpp` - Tool management system
- `include/kolosal/agents/memory_manager.hpp` - Memory management
- `include/kolosal/agents/planning_system.hpp` - Planning and reasoning
- `include/kolosal/agents/agent_factory.hpp` - Agent creation utilities

## 🤝 Contributing

To add new features or improvements:
1. Follow the existing architecture patterns
2. Add comprehensive tests for new functionality
3. Update documentation and examples
4. Ensure backward compatibility with existing code

---

These advanced features transform the Kolosal Agent System into a comprehensive, enterprise-ready platform that rivals the capabilities of leading agent frameworks while maintaining the performance and reliability of the C++ implementation.

# Kolosal Agents

This document provides an overview of the **agent** system in Kolosal Server, including the core concepts, architecture, and how to configure and use agents in your multi-agent system.

## What is an Agent?

An **Agent** in Kolosal is an autonomous, modular component capable of executing functions, handling messages, and collaborating with other agents. Agents can be configured with specific capabilities and functions, and communicate via a sophisticated message routing system with support for workflow orchestration.

## Key Components

### Core Architecture

- **AgentCore**: The main class representing an individual agent. Handles lifecycle (start/stop), function execution, message handling, and capability management. Each agent has a unique UUID and supports concurrent job execution.
- **YAMLConfigurableAgentManager**: The system-wide agent manager that loads agent and function configurations from YAML files, manages agent lifecycle, supports hot-reloading, and provides centralized agent orchestration.
- **AgentOrchestrator**: Advanced workflow orchestration system that coordinates multi-agent collaboration, manages complex workflows, and handles inter-agent dependencies.
- **FunctionManager**: Registers and manages functions that agents can execute. Supports multiple function types:
  - **Builtin functions**: Native server functions (inference, text_processing, data_analysis, retrieval)
  - **LLM functions**: Functions that leverage language models for processing
  - **External API functions**: Integration with external services (web_search, etc.)
  - **Custom functions**: User-defined function implementations
  - **Retrieval functions**: Vector search and document retrieval capabilities
- **JobManager**: Handles asynchronous job execution with priority queuing, job status tracking, and result management.
- **EventSystem**: Comprehensive event handling system that emits and processes agent-related events (message received, function executed, job completed, etc.).
- **MessageRouter**: Advanced message routing system supporting direct messaging, broadcasting, priority handling, and correlation tracking.
- **DocumentService**: Manages document indexing, embedding generation, and vector storage for retrieval-augmented generation (RAG) capabilities.

## Agent Lifecycle

1. **Configuration Loading**: Agents are created from YAML configuration files or programmatically via API calls, with support for multiple agent types and specialized roles.
2. **Initialization**: Each agent receives a unique UUID, name, type, and role configuration. The system supports the following predefined agent types:
   - **Research Agents**: Information gathering, analysis, and synthesis
   - **Development Agents**: Code generation, review, debugging, and optimization
   - **Analytics Agents**: Data analysis, visualization, and statistical processing
   - **Creative Agents**: Content creation, writing, and copywriting
   - **Management Agents**: Project coordination, task management, and progress tracking
   - **QA Agents**: Quality assurance, testing, and validation
3. **Capability Assignment**: Agents are assigned specific capabilities based on their type and configuration (e.g., text_processing, code_generation, data_analysis, web_search).
4. **Function Registration**: Functions are registered to agents based on their type and capabilities. Functions can be:
   - **Built-in**: Native server functions (inference, text_processing, data_analysis, retrieval)
   - **LLM-powered**: Functions that use language models with customizable prompts and parameters
   - **External API**: Integration with external services (web_search, etc.)
   - **Custom**: User-defined implementations
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
  log_level: "debug"
  global_settings:
    default_timeout_ms: 60000
    max_retries: 3
    enable_metrics: true

# Inference engines configuration
inference_engines:
  - name: "default"
    type: "llama_cpp"
    model_path: "downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf"
    settings:
      context_size: 4096
      batch_size: 512
      threads: 4
      gpu_layers: 0
    auto_load: true

# Function definitions
functions:
  - name: "inference"
    type: "builtin"
    description: "Generate text using the inference engine"
    parameters:
      prompt: "Input prompt for text generation"
      max_tokens: "Maximum tokens to generate"
      temperature: "Temperature for text generation"
    timeout_ms: 60000
    
  - name: "web_search"
    type: "external_api"
    description: "Search the web for information"
    endpoint: "https://api.search.com/v1/search"
    parameters:
      query: "Search query"
      limit: "Number of results"
    timeout_ms: 60000
    
  - name: "retrieval"
    type: "builtin"
    description: "Search and retrieve relevant documents from knowledge base"
    parameters:
      query: "Search query for document retrieval"
      k: "Number of documents to retrieve"
      score_threshold: "Minimum similarity score threshold"
      collection_name: "Collection name (optional)"
    timeout_ms: 60000
    
  - name: "context_retrieval"
    type: "builtin"
    description: "Retrieve and format documents as context for enhanced responses"
    parameters:
      query: "Search query for context retrieval"
      k: "Number of documents to retrieve"
      context_format: "Format for context (summary or detailed)"
      collection_name: "Collection name (optional)"
    timeout_ms: 60000
    
  - name: "add_document"
    type: "builtin"
    description: "Add documents to the knowledge base"
    parameters:
      documents: "Array of documents to add"
      collection_name: "Collection name (optional)"
    timeout_ms: 120000
    
  - name: "remove_document"
    type: "builtin"
    description: "Remove documents from the knowledge base"
    parameters:
      document_ids: "Array of document IDs to remove"
      collection_name: "Collection name (optional)"
    timeout_ms: 60000

# Agent definitions
agents:
  - name: "research_assistant"
    type: "research"
    role: "Information researcher and analyzer"
    system_prompt: >
      You are a research assistant specialized in gathering, analyzing, and summarizing information.
      Your goal is to provide accurate, well-researched, and comprehensive answers to queries.
      Always cite sources when possible and indicate confidence levels in your findings.
    capabilities:
      - "web_search"
      - "text_processing" 
      - "data_analysis"
      - "information_synthesis"
      - "retrieval"
      - "context_retrieval"    
    functions:
      - "inference"
      - "web_search"
      - "text_processing"
      - "data_analysis"
      - "retrieval"
      - "context_retrieval"    
    llm_config:
      model_name: "test-qwen-0.6b"
      api_endpoint: "http://localhost:8080/v1"
      instruction: "You are a research assistant. Provide accurate, well-researched answers."
      temperature: 0.3
      max_tokens: 2048
      timeout_seconds: 120
      max_retries: 3
    auto_start: true
    max_concurrent_jobs: 3
    heartbeat_interval_seconds: 10
    custom_settings:
      search_depth: "comprehensive"
      fact_checking: "enabled"
      
  - name: "code_assistant"
    type: "development"
    role: "Software development assistant"
    system_prompt: >
      You are a senior software developer assistant. You help with code generation, review, 
      debugging, and optimization. You follow best practices and write clean, maintainable code.
    capabilities:
      - "code_generation"
      - "code_review"
      - "debugging"
      - "optimization"    
    functions:
      - "inference"
      - "code_generation"
      - "text_processing"
    llm_config:
      model_name: "test-qwen-0.6b"
      api_endpoint: "http://localhost:8080/v1"
      instruction: "You are a software development assistant. Write clean, maintainable code."
      temperature: 0.2
      max_tokens: 4096
      timeout_seconds: 60
      max_retries: 2
    auto_start: true
    max_concurrent_jobs: 2
    heartbeat_interval_seconds: 15
```

## Message Types

The agent system supports comprehensive message types for sophisticated inter-agent communication:

### Core Message Types
- **`ping` / `pong`**: Health check and heartbeat between agents
- **`greeting`**: Introduction and capability discovery messages
- **`task_request`**: Request for task execution with priority and correlation ID
- **`task_response`**: Response with task results, status, and execution metadata
- **`function_request`**: Direct function execution request with parameters
- **`function_response`**: Function execution response with results and timing
- **`workflow_request`**: Multi-step workflow coordination request
- **`workflow_update`**: Workflow progress and status updates

### Advanced Message Types
- **`collaboration_request`**: Request for multi-agent collaboration
- **`resource_sharing`**: Resource allocation and sharing between agents
- **`status_update`**: Agent status and health information
- **`error_notification`**: Error reporting and exception handling
- **`metric_report`**: Performance metrics and monitoring data
- **`configuration_update`**: Dynamic configuration changes

### Message Structure
```json
{
  "id": "msg_12345",
  "from_agent": "research_assistant",
  "to_agent": "code_assistant",
  "type": "task_request",
  "payload": {
    "task": "Generate API documentation",
    "priority": "high",
    "deadline": "2025-01-01T12:00:00Z",
    "context": {...}
  },
  "timestamp": "2025-06-24T10:30:00Z",
  "priority": 1,
  "correlation_id": "workflow_67890"
}
```

## API Integration

The agent system provides comprehensive REST API endpoints for management and interaction:

### Agent Management
- **`GET /api/v1/agents`**: List all agents with status and capabilities
- **`GET /api/v1/agents/{agent_id}`**: Get detailed agent information
- **`POST /api/v1/agents`**: Create new agent from configuration
- **`POST /api/v1/agents/{agent_id}/start`**: Start a specific agent
- **`POST /api/v1/agents/{agent_id}/stop`**: Stop a specific agent
- **`DELETE /api/v1/agents/{agent_id}`**: Remove agent from system

### Function Execution
- **`POST /api/v1/agents/{agent_id}/execute`**: Execute function synchronously
- **`POST /api/v1/agents/{agent_id}/execute-async`**: Execute function asynchronously
- **`GET /api/v1/agents/jobs/{job_id}/status`**: Get job execution status
- **`GET /api/v1/agents/jobs/{job_id}/result`**: Get job execution results

### Communication
- **`POST /api/v1/agents/messages/send`**: Send message between agents
- **`POST /api/v1/agents/messages/broadcast`**: Broadcast message to all agents
- **`GET /api/v1/agents/{agent_id}/messages`**: Get agent message history

### System Management
- **`GET /api/v1/agents/system/status`**: Get system-wide status
- **`POST /api/v1/agents/system/reload`**: Reload configuration
- **`GET /api/v1/agents/system/metrics`**: Get performance metrics

### Document & Retrieval Management
- **`POST /retrieve`**: Retrieve documents using vector search
- **`POST /api/v1/documents`**: Add documents to knowledge base
- **`DELETE /api/v1/documents`**: Remove documents from knowledge base
- **`GET /api/v1/documents/collections`**: List available collections
- **`POST /parse-pdf`**: Parse PDF documents for indexing
- **`POST /parse-docx`**: Parse DOCX documents for indexing

### Workflow Orchestration
- **`POST /api/v1/orchestration/workflows`**: Create complex workflows
- **`GET /api/v1/orchestration/workflows/{workflow_id}`**: Get workflow status
- **`POST /api/v1/orchestration/workflows/{workflow_id}/execute`**: Execute workflow
- **`GET /api/v1/orchestration/metrics`**: Get orchestration metrics
- **`GET /api/v1/orchestration/status`**: Get orchestrator status

## Extending Agents

### Adding New Functions
Implement the `AgentFunction` interface and register with the `FunctionManager`:

```cpp
class CustomFunction : public AgentFunction {
public:
    std::string get_name() const override { return "custom_processing"; }
    std::string get_description() const override { return "Custom data processing"; }
    std::string get_type() const override { return "custom"; }
    
    FunctionResult execute(const AgentData& params) override {
        // Implementation here
        FunctionResult result(true);
        result.result_data.set("processed", "Custom processing complete");
        return result;
    }
};
```

### Creating Custom Agent Types
Define new agent types in YAML configuration with specialized capabilities:

```yaml
agents:
  - name: "custom_specialist"
    type: "custom"
    role: "Specialized custom processing agent"
    capabilities: ["custom_processing", "specialized_analysis"]
    functions: ["custom_processing", "inference"]
    # ... additional configuration
```

### Integrating External APIs
Configure external API functions in the system:

```yaml
functions:
  - name: "external_service"
    type: "external_api"
    description: "Integration with external service"
    endpoint: "https://api.example.com/v1/process"
    parameters:
      api_key: "Your API key"
      timeout_ms: 30000
```

## Advanced Features

### Retrieval-Augmented Generation (RAG)
The system includes comprehensive RAG capabilities:

```yaml
# Database configuration for RAG
database:
  qdrant:
    enabled: true
    host: "localhost"
    port: 6333
    apiKey: ""
    timeout: 30
    maxConnections: 10
    connectionTimeout: 5
    defaultEmbeddingModel: "default"
```

#### Document Management
- **Document Indexing**: Automatic embedding generation and vector storage
- **Multi-format Support**: PDF, DOCX, and text document processing
- **Vector Search**: Semantic similarity search with configurable thresholds
- **Context Enhancement**: Automatic context formatting for LLM consumption

#### Retrieval Functions
- **`retrieval`**: Basic document search and retrieval
- **`context_retrieval`**: Enhanced context formatting for AI responses
- **`add_document`**: Document indexing and storage
- **`remove_document`**: Document removal and cleanup

### Workflow Orchestration
The `AgentOrchestrator` enables complex multi-agent workflows:

```json
{
  "name": "RAG-Enhanced Content Creation",
  "description": "Research, retrieve context, and create content",
  "global_context": {
    "topic": "AI in Healthcare",
    "target_audience": "medical professionals",
    "use_rag": true
  },
  "steps": [
    {
      "name": "knowledge_retrieval",
      "agent": "research_assistant",
      "function": "retrieval",
      "parameters": {
        "query": "AI healthcare applications",
        "k": 10,
        "score_threshold": 0.7
      }
    },
    {
      "name": "context_enhancement",
      "agent": "research_assistant", 
      "function": "context_retrieval",
      "parameters": {
        "query": "AI healthcare benefits and challenges",
        "k": 5,
        "context_format": "detailed"
      }
    },
    {
      "name": "content_creation",
      "agent": "content_creator",
      "function": "inference",
      "parameters": {
        "prompt": "Using the retrieved context, write a comprehensive article about AI in healthcare",
        "context": "{{context_enhancement.result}}"
      },
      "depends_on": ["knowledge_retrieval", "context_enhancement"]
    }
  ]
}
```
      "depends_on": ["research"]
    },
    {
      "name": "review",
      "agent": "qa_specialist",
      "function": "text_processing",
      "depends_on": ["writing"]
    }
  ]
}
```

### Performance Monitoring
Built-in metrics and monitoring:
- Agent performance statistics
- Function execution times
- Message routing efficiency
- Job queue status
- Resource utilization

### High Availability
- Automatic agent restart on failure
- Load balancing across agent instances
- Graceful degradation mechanisms
- Configuration hot-reloading

### Security Features
- Agent isolation and sandboxing
- Function execution limits
- Resource usage monitoring
- Audit logging

## Demo & Status

The system supports comprehensive demonstration and status reporting:

- **`demonstrate_system()`**: Shows system capabilities and agent status
- **Real-time metrics**: Performance monitoring and health checks
- **Interactive testing**: API endpoints for testing agent functionality
- **Configuration validation**: YAML configuration validation and error reporting

## Example Usage

### Creating and Managing Agents via API

```bash
# List all agents
curl -X GET http://localhost:8080/api/v1/agents

# Create a new agent with RAG capabilities
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "rag_agent",
    "type": "research",
    "capabilities": ["retrieval", "context_retrieval", "text_processing"],
    "functions": ["inference", "retrieval", "context_retrieval"],
    "auto_start": true
  }'

# Execute retrieval function
curl -X POST http://localhost:8080/api/v1/agents/rag_agent/execute \
  -H "Content-Type: application/json" \
  -d '{
    "function": "retrieval",
    "parameters": {
      "query": "machine learning algorithms",
      "k": 5,
      "score_threshold": 0.6
    }
  }'

# Add documents to knowledge base
curl -X POST http://localhost:8080/api/v1/documents \
  -H "Content-Type: application/json" \
  -d '{
    "documents": [
      {
        "text": "Machine learning is a subset of artificial intelligence...",
        "metadata": {
          "source": "ML_Guide.pdf",
          "page": 1,
          "category": "introduction"
        }
      }
    ]
  }'

# Retrieve documents using vector search
curl -X POST http://localhost:8080/retrieve \
  -H "Content-Type: application/json" \
  -d '{
    "query": "what is machine learning",
    "k": 3,
    "score_threshold": 0.5
  }'

# Parse and index PDF document
curl -X POST http://localhost:8080/parse-pdf \
  -H "Content-Type: application/json" \
  -d '{
    "pdf_data": "base64_encoded_pdf_content",
    "method": "fast",
    "auto_index": true
  }'

# Send a message between agents
curl -X POST http://localhost:8080/api/v1/agents/messages/send \
  -H "Content-Type: application/json" \
  -d '{
    "from_agent": "research_assistant",
    "to_agent": "code_assistant",
    "type": "task_request",
    "payload": {
      "task": "Generate documentation",
      "priority": "high"
    }
  }'
```

---

For more details, see the [Agent System API Documentation](AGENT_SYSTEM_API.md) and [Architecture Overview](ARCHITECTURE.md).

# Kolosal Agents

This document provides an overview of the **agent** system in Kolosal Server, including the core concepts, architecture, and how to configure and use agents in your multi-agent system.

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
- **`document_notification`**: Document-related events and updates

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
- **`PUT /api/v1/agents/{agent_id}`**: Update agent configuration
- **`POST /api/v1/agents/{agent_id}/start`**: Start a specific agent
- **`POST /api/v1/agents/{agent_id}/stop`**: Stop a specific agent
- **`DELETE /api/v1/agents/{agent_id}`**: Remove agent from system

### Function Execution
- **`POST /api/v1/agents/{agent_id}/functions/{function_name}`**: Execute function synchronously
- **`POST /api/v1/agents/{agent_id}/inference`**: Direct inference endpoint for agents
- **`GET /api/v1/agents/{agent_id}/functions`**: List available functions for agent
- **`GET /api/v1/agents/{agent_id}/capabilities`**: Get agent capabilities

### Communication
- **`POST /api/v1/agents/{agent_id}/message`**: Send message to specific agent
- **`POST /api/v1/agents/messages/broadcast`**: Broadcast message to all agents
- **`GET /api/v1/agents/{agent_id}/messages`**: Get agent message history

### System Management
- **`GET /api/v1/agents/system/status`**: Get system-wide status
- **`GET /api/v1/agents/system/metrics`**: Get performance metrics
- **`POST /api/v1/agents/system/reload`**: Reload configuration

### Document & Retrieval Management (Enhanced RAG)
- **`POST /retrieve`**: Retrieve documents using vector search
- **`POST /api/v1/documents`**: Add documents to knowledge base
- **`DELETE /api/v1/documents`**: Remove documents from knowledge base
- **`GET /api/v1/agents/collections`**: List available collections
- **`POST /api/v1/agents/collections`**: Create new collection
- **`GET /api/v1/agents/collections/{collection_name}`**: Get collection info
- **`DELETE /api/v1/agents/collections/{collection_name}`**: Delete collection
- **`POST /parse-pdf`**: Parse PDF documents for indexing
- **`POST /parse-docx`**: Parse DOCX documents for indexing

### Workflow Management (Sequential Workflows)
- **`GET /api/v1/sequential/workflows`**: List sequential workflows
- **`POST /api/v1/sequential/workflows`**: Create new sequential workflow
- **`GET /api/v1/sequential/workflows/{workflow_id}`**: Get workflow details
- **`POST /api/v1/sequential/workflows/{workflow_id}/execute`**: Execute workflow
- **`GET /api/v1/sequential/workflows/{workflow_id}/status`**: Get workflow status
- **`DELETE /api/v1/sequential/workflows/{workflow_id}`**: Delete workflow

### Advanced Workflow Orchestration
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
The system includes comprehensive RAG capabilities with enhanced document processing:

```yaml
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
```

#### Document Management Features
- **Multi-format Support**: PDF, DOCX, and text document processing with automatic content extraction
- **Embedding Generation**: Automatic embedding generation using configured embedding models
- **Vector Storage**: Semantic similarity search with Qdrant vector database integration
- **Collection Management**: Multiple collections for organizing different types of documents
- **Batch Processing**: Efficient bulk document operations with progress tracking
- **Quality Validation**: Automatic validation of document content and metadata

#### Enhanced Retrieval Functions
- **`retrieval`**: Basic semantic document search with configurable similarity thresholds
- **`context_retrieval`**: Enhanced context formatting optimized for LLM consumption
- **`add_document`**: Document indexing with automatic chunking and metadata extraction
- **`remove_document`**: Document removal with cleanup of related embeddings
- **`parse_pdf`**: PDF content extraction with text, metadata, and structure preservation
- **`parse_docx`**: DOCX processing with formatting and style information
- **`get_embedding`**: Direct embedding generation for custom text content
- **`test_document_service`**: Connection testing and system validation

### Advanced Workflow Engine
The `WorkflowEngine` enables sophisticated multi-step workflows with enhanced capabilities:

#### Workflow Types
- **Sequential**: Steps execute one after another in defined order
- **Parallel**: Multiple steps execute simultaneously for improved efficiency
- **Pipeline**: Data flows seamlessly from step to step with context preservation
- **Consensus**: Multiple agents collaborate to reach consensus on decisions
- **Conditional**: Dynamic step execution based on runtime conditions and results

#### Error Handling & Recovery
- **Retry Mechanisms**: Configurable retry strategies with exponential backoff
- **Fallback Agents**: Automatic failover to backup agents when primary agents fail
- **Checkpoint Recovery**: State persistence for workflow resumption after interruptions
- **Partial Failure Handling**: Continue execution with partial results when appropriate

#### Enhanced Orchestration Features
```json
{
  "name": "Advanced RAG Research Pipeline",
  "description": "Multi-stage research with document retrieval and synthesis",
  "type": "PIPELINE",
  "steps": [
    {
      "step_id": "document_search",
      "agent_id": "research_assistant",
      "function_name": "retrieval",
      "parameters": {
        "query": "{{global_context.research_topic}}",
        "k": 15,
        "score_threshold": 0.7,
        "collection_name": "research_papers"
      },
      "timeout_seconds": 45,
      "max_retries": 3
    },
    {
      "step_id": "context_synthesis",
      "agent_id": "knowledge_agent",
      "function_name": "context_retrieval",
      "parameters": {
        "query": "synthesize research findings",
        "k": 10,
        "context_format": "detailed",
        "input_documents": "{{document_search.output}}"
      },
      "dependencies": [
        {"step_id": "document_search", "condition": "success"}
      ],
      "timeout_seconds": 60
    },
    {
      "step_id": "comprehensive_analysis",
      "agent_id": "data_analyst",
      "function_name": "inference",
      "parameters": {
        "prompt": "Analyze the synthesized research and provide insights",
        "context": "{{context_synthesis.output}}",
        "max_tokens": 2048,
        "temperature": 0.3
      },
      "dependencies": [
        {"step_id": "context_synthesis", "condition": "success"}
      ],
      "timeout_seconds": 90
    }
  ],
  "error_handling": {
    "retry_on_failure": true,
    "max_retries": 3,
    "use_fallback_agent": true,
    "continue_on_error": false
  }
}
```
### Performance Monitoring & Analytics
Built-in comprehensive metrics and monitoring:

#### Agent Performance Metrics
- **Execution Statistics**: Function call counts, success rates, and average execution times
- **Resource Utilization**: Memory usage, CPU consumption, and thread utilization
- **Health Monitoring**: Heartbeat tracking, error rates, and availability metrics
- **Workload Analysis**: Job queue depths, concurrent execution tracking

#### System-Wide Analytics
- **Workflow Performance**: End-to-end workflow execution times and success rates
- **Document Service Metrics**: Retrieval performance, embedding generation times, search accuracy
- **Inter-Agent Communication**: Message routing efficiency, collaboration patterns
- **Capacity Planning**: Resource usage trends and scaling recommendations

### High Availability & Reliability
- **Automatic Recovery**: Agent restart on failure with state preservation
- **Load Distribution**: Intelligent workload distribution across available agents
- **Graceful Degradation**: System continues operating with reduced capacity during failures
- **Configuration Hot-Reloading**: Update agent configurations without system restart
- **State Persistence**: Workflow and execution state preserved across system restarts

### Security & Compliance
- **Agent Isolation**: Sandboxed execution environments for secure function execution
- **Function Access Control**: Role-based access to functions and capabilities
- **Resource Limits**: Configurable execution timeouts and resource consumption limits
- **Audit Logging**: Comprehensive logging of all agent activities and system events
- **Input Validation**: Robust validation and sanitization of all inputs and parameters

## Demo & Status

The system supports comprehensive demonstration and status reporting:

- **`demonstrate_system()`**: Shows system capabilities and agent status
- **Real-time metrics**: Performance monitoring and health checks
- **Interactive testing**: API endpoints for testing agent functionality
- **Configuration validation**: YAML configuration validation and error reporting

## Example Usage

### Creating and Managing Agents via API

```bash
# List all agents with detailed information
curl -X GET http://localhost:8080/api/v1/agents

# Create a new RAG-enabled research agent
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "rag_researcher",
    "type": "research",
    "role": "RAG-enhanced research assistant",
    "system_prompt": "You are a research assistant that uses document retrieval to provide accurate, context-rich responses.",
    "capabilities": [
      "document_retrieval", "context_retrieval", "text_processing", 
      "information_synthesis", "document_management"
    ],
    "functions": [
      "inference", "retrieval", "context_retrieval", "text_processing",
      "add_document", "parse_pdf", "parse_docx"
    ],
    "llm_config": {
      "api_endpoint": "http://localhost:8080/v1",
      "temperature": 0.3,
      "max_tokens": 2048
    },
    "auto_start": true,
    "max_concurrent_jobs": 4
  }'

# Execute document retrieval function
curl -X POST http://localhost:8080/api/v1/agents/rag_researcher/functions/retrieval \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {
      "query": "machine learning in healthcare",
      "k": 10,
      "score_threshold": 0.6,
      "collection_name": "medical_research"
    }
  }'

# Execute RAG-enhanced inference
curl -X POST http://localhost:8080/api/v1/agents/rag_researcher/functions/rag_inference \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {
      "prompt": "What are the latest developments in AI for medical diagnosis?",
      "query": "AI medical diagnosis recent developments",
      "k": 8,
      "max_tokens": 1024,
      "temperature": 0.3
    }
  }'

# Direct inference endpoint
curl -X POST http://localhost:8080/api/v1/agents/rag_researcher/inference \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "Summarize the key benefits of AI in healthcare",
    "max_tokens": 500,
    "temperature": 0.3
  }'

# Add documents to knowledge base using agent
curl -X POST http://localhost:8080/api/v1/agents/document_manager/functions/add_document \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {
      "documents": [
        {
          "text": "Artificial intelligence in healthcare has shown remarkable progress...",
          "metadata": {
            "source": "Medical_AI_Review_2024.pdf",
            "category": "healthcare_ai",
            "date": "2024-01-15"
          }
        }
      ],
      "collection_name": "medical_research"
    }
  }'

# Parse and index PDF document
curl -X POST http://localhost:8080/parse-pdf \
  -H "Content-Type: application/json" \
  -d '{
    "pdf_data": "base64_encoded_pdf_content",
    "method": "comprehensive",
    "auto_index": true,
    "collection_name": "medical_research",
    "metadata": {
      "source": "research_paper.pdf",
      "category": "clinical_studies"
    }
  }'
```

### Advanced Multi-Agent Workflow Example

```bash
# Create a comprehensive research and analysis workflow
curl -X POST http://localhost:8080/api/v1/sequential/workflows \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Comprehensive Medical Research Analysis",
    "description": "Multi-stage workflow for medical research analysis with RAG",
    "steps": [
      {
        "step_id": "document_retrieval",
        "agent_id": "rag_researcher",
        "function_name": "retrieval",
        "parameters": {
          "query": "AI medical diagnosis accuracy studies",
          "k": 15,
          "score_threshold": 0.65,
          "collection_name": "medical_research"
        },
        "timeout_seconds": 60
      },
      {
        "step_id": "context_synthesis",
        "agent_id": "knowledge_agent",
        "function_name": "context_retrieval",
        "parameters": {
          "query": "medical AI diagnostic accuracy trends",
          "k": 10,
          "context_format": "detailed"
        },
        "dependencies": ["document_retrieval"]
      },
      {
        "step_id": "data_analysis",
        "agent_id": "data_analyst",
        "function_name": "inference",
        "parameters": {
          "prompt": "Analyze the medical research data and identify key trends in AI diagnostic accuracy",
          "context": "{{context_synthesis.output}}",
          "max_tokens": 2048,
          "temperature": 0.2
        },
        "dependencies": ["context_synthesis"]
      },
      {
        "step_id": "content_creation",
        "agent_id": "content_creator", 
        "function_name": "inference",
        "parameters": {
          "prompt": "Create a comprehensive report based on the analysis",
          "context": "{{data_analysis.output}}",
          "max_tokens": 3072,
          "temperature": 0.4
        },
        "dependencies": ["data_analysis"]
      },
      {
        "step_id": "quality_review",
        "agent_id": "qa_specialist",
        "function_name": "inference",
        "parameters": {
          "prompt": "Review the report for accuracy, completeness, and clarity",
          "context": "{{content_creation.output}}",
          "max_tokens": 1024,
          "temperature": 0.1
        },
        "dependencies": ["content_creation"]
      }
    ],
    "global_context": {
      "research_domain": "medical_ai",
      "output_format": "comprehensive_report",
      "quality_standards": "high"
    }
  }'

# Execute the workflow
curl -X POST http://localhost:8080/api/v1/sequential/workflows/workflow_123/execute \
  -H "Content-Type: application/json" \
  -d '{
    "input_context": {
      "research_focus": "diagnostic_accuracy",
      "target_audience": "medical_professionals",
      "urgency": "standard"
    }
  }'

# Monitor workflow progress
curl http://localhost:8080/api/v1/sequential/workflows/workflow_123/status
```

### Document Management and RAG Operations

```bash
# Test document service connectivity
curl -X POST http://localhost:8080/api/v1/agents/document_manager/functions/test_document_service \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {}
  }'

# Create a new document collection
curl -X POST http://localhost:8080/api/v1/agents/collections \
  -H "Content-Type: application/json" \
  -d '{
    "collection_name": "clinical_studies",
    "description": "Clinical research studies and trials",
    "metadata_schema": {
      "study_type": "string",
      "publication_date": "string", 
      "institution": "string",
      "peer_reviewed": "boolean"
    }
  }'

# Bulk document processing workflow
curl -X POST http://localhost:8080/api/v1/agents/workflows \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Bulk Document Processing",
    "type": "document_processing",
    "steps": [
      {
        "agent_id": "document_manager",
        "function": "parse_pdf",
        "parameters": {
          "batch_mode": true,
          "auto_index": true,
          "quality_validation": true
        }
      },
      {
        "agent_id": "document_manager", 
        "function": "add_document",
        "parameters": {
          "collection_name": "clinical_studies",
          "batch_processing": true
        }
      }
    ]
  }'

# Advanced semantic search with filtering
curl -X POST http://localhost:8080/retrieve \
  -H "Content-Type: application/json" \
  -d '{
    "query": "randomized controlled trial diabetes treatment",
    "k": 20,
    "score_threshold": 0.7,
    "collection_name": "clinical_studies",
    "metadata_filter": {
      "study_type": "RCT",
      "peer_reviewed": true
    },
    "include_metadata": true,
    "rerank": true
  }'
```

### System Monitoring and Management

```bash
# Get comprehensive system status
curl http://localhost:8080/api/v1/agents/system/status

# Get detailed performance metrics
curl http://localhost:8080/api/v1/agents/system/metrics

# Get workflow engine metrics
curl http://localhost:8080/api/v1/orchestration/metrics

# List all available agent functions
curl http://localhost:8080/api/v1/agents/research_assistant/functions

# Broadcast system-wide message
curl -X POST http://localhost:8080/api/v1/agents/messages/broadcast \
  -H "Content-Type: application/json" \
  -d '{
    "from_agent": "system_manager",
    "type": "system_announcement",
    "payload": {
      "message": "System maintenance scheduled for tonight",
      "maintenance_window": "2024-12-09T02:00:00Z to 2024-12-09T04:00:00Z",
      "affected_services": ["document_service", "workflow_engine"],
      "expected_impact": "minimal"
    }
  }'
```

---

For more details, see the [Agent System API Documentation](AGENT_SYSTEM_API.md) and [Architecture Overview](ARCHITECTURE.md).

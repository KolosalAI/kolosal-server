# Agent System API Documentation

## Overview

The Kolosal Server Agent System provides a comprehensive multi-agent framework that enables:
- Dynamic agent creation and management with unique ID-based identification
- Inter-agent communication and coordination with message correlation
- Advanced workflow orchestration and automation with dependency management
- Collaborative problem-solving patterns with consensus mechanisms
- Load balancing and optimization with automatic resource management
- Real-time monitoring and performance metrics
- Hot-reloading configuration without system restart
- **Enhanced Retrieval-Augmented Generation (RAG)** with semantic vector search capabilities
- **Comprehensive document management** with multi-format support (PDF, DOCX, text)
- **Intelligent semantic search** with configurable similarity thresholds and metadata filtering
- **Context-aware enhancement** for improved AI responses with document integration
- **Sequential and advanced workflow engines** for complex multi-step processing

## Architecture

### Core Components

1. **AgentCore** - Individual agent implementation with thread-safe execution and unique ID identification
2. **YAMLConfigurableAgentManager** - System-wide agent management with hot-reloading support
3. **AgentOrchestrator** - Advanced workflow and collaboration orchestration with dependency resolution
4. **WorkflowEngine** - Sequential, parallel, pipeline, consensus, and conditional workflow support
5. **MessageRouter** - Inter-agent communication with priority queuing and correlation tracking
6. **FunctionManager** - Function execution management supporting builtin, LLM, external API, and custom functions
7. **JobManager** - Asynchronous job handling with priority queues and status tracking
8. **EventSystem** - Real-time event processing and notification system
9. **ConfigurableAgentFactory** - Factory for creating agents from YAML configurations

### Agent Types

The system supports 8 predefined agent types with specialized capabilities:

- **Research Agents** (`research`) - Information gathering, web search, document retrieval, data analysis, and synthesis
- **Development Agents** (`development`) - Code generation, review, debugging, and optimization
- **Analytics Agents** (`analytics`) - Data analysis, statistical processing, and visualization
- **Creative Agents** (`creative`) - Content creation, writing, copywriting, and creative tasks
- **Management Agents** (`management`) - Project coordination, task management, and progress tracking
- **QA Agents** (`quality_assurance`) - Quality assurance, testing, validation, and process improvement
- **Document Management** (`document_management`) - Document processing, knowledge base management, and content organization
- **General Purpose** (`general`) - Flexible agents for various tasks and testing purposes

### Function Types

- **Builtin Functions** - Native server functions (inference, text_processing, data_analysis, retrieval, context_retrieval, add_document, remove_document, parse_pdf, parse_docx, get_embedding, test_document_service)
- **LLM Functions** - AI-powered functions using language models with configurable prompts
- **External API Functions** - Integration with external services (web_search, etc.)
- **Custom Functions** - User-defined function implementations
- **Document Processing Functions** - PDF/DOCX parsing, embedding generation, and document management
- **RAG Functions** - Enhanced retrieval and context-aware processing for knowledge-based AI responses

## API Endpoints

### Agent Management

#### List All Agents
```http
GET /api/v1/agents
```

**Response:**
```json
{
  "success": true,
  "data": [
    {
      "agent_id": "research_assistant",
      "name": "research_assistant",
      "type": "research",
      "capabilities": [
        "web_search", 
        "text_processing", 
        "data_analysis",
        "information_synthesis",
        "document_retrieval",
        "context_retrieval",
        "document_management",
        "document_parsing"
      ],
      "running": true
    },
    {
      "agent_id": "document_manager",
      "name": "document_manager", 
      "type": "document_management",
      "capabilities": [
        "document_management",
        "document_parsing", 
        "document_retrieval",
        "knowledge_base_management",
        "text_processing"
      ],
      "running": true
    }
  ],
  "count": 2
}
```

#### Get Agent Details
```http
GET /api/v1/agents/{agent_id}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "research_assistant",
    "name": "research_assistant",
    "type": "research",
    "role": "Information researcher and analyzer",
    "system_prompt": "You are a research assistant specialized in information gathering and analysis.",
    "capabilities": [
      "web_search",
      "text_processing", 
      "data_analysis",
      "information_synthesis",
      "document_retrieval",
      "context_retrieval",
      "document_management",
      "document_parsing"
    ],
    "running": true,
    "functions": [
      "inference",
      "web_search", 
      "text_processing",
      "data_analysis",
      "retrieval",
      "context_retrieval",
      "add_document",
      "remove_document",
      "parse_pdf",
      "parse_docx",
      "get_embedding",
      "test_document_service"
    ],
    "max_concurrent_jobs": 3,
    "heartbeat_interval_seconds": 10,
    "llm_config": {
      "api_endpoint": "http://localhost:8080/v1",
      "temperature": 0.3,
      "max_tokens": 2048,
      "timeout_seconds": 120,
      "max_retries": 3
    },
    "custom_settings": {
      "fact_checking": "enabled",
      "search_depth": "comprehensive"
    },
    "statistics": {
      "total_jobs_executed": 47,
      "successful_jobs": 45,
      "failed_jobs": 2,
      "average_execution_time_ms": 1234.5,
      "uptime_seconds": 3600
    }
  }
}
```

#### Create New Agent
```http
POST /api/v1/agents
```

**Request Body:**
```json
{
  "name": "rag_specialist",
  "type": "research",
  "role": "RAG-enhanced research specialist",
  "system_prompt": "You are a research specialist that uses document retrieval to provide accurate, context-rich responses.",
  "capabilities": [
    "document_retrieval",
    "context_retrieval",
    "text_processing",
    "data_analysis",
    "document_management",
    "document_parsing"
  ],
  "functions": [
    "inference",
    "retrieval",
    "context_retrieval",
    "text_processing",
    "data_analysis",
    "add_document",
    "remove_document",
    "parse_pdf",
    "parse_docx",
    "get_embedding"
  ],
  "llm_config": {
    "api_endpoint": "http://localhost:8080/v1",
    "instruction": "You are a research specialist that uses retrieved context to provide accurate answers.",
    "temperature": 0.3,
    "max_tokens": 2048,
    "timeout_seconds": 60,
    "max_retries": 3
  },
  "auto_start": true,
  "max_concurrent_jobs": 4,
  "heartbeat_interval_seconds": 10,
  "custom_settings": {
    "enable_logging": true,
    "priority_level": "high",
    "workflow_enabled": true,
    "collaboration_enabled": true,
    "rag_enabled": true,
    "context_window": "large"
  },
  "metadata": {
    "created_by": "user123",
    "version": "2.0",
    "tags": ["research", "rag", "document_processing"]
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "rag_specialist_12345",
    "status": "created",
    "message": "RAG-enhanced agent created successfully"
  }
}
```

#### Start Agent
```http
POST /api/v1/agents/{agent_id}/start
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "research_assistant",
    "status": "started",
    "message": "Agent started successfully"
  }
}
```

#### Stop Agent
```http
POST /api/v1/agents/{agent_id}/stop
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "research_assistant", 
    "status": "stopped",
    "message": "Agent stopped successfully"
  }
}
```

#### Delete Agent
```http
DELETE /api/v1/agents/{agent_id}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "research_assistant",
    "status": "deleted",
    "message": "Agent deleted successfully"
  }
}
```

### Agent Communication

#### Send Message to Agent
```http
POST /api/v1/agents/messages/send
```

**Request Body:**
```json
{
  "from_agent": "research_assistant",
  "to_agent": "code_assistant",
  "type": "task_request",
  "payload": {
    "task": "Generate code based on research findings",
    "data": "Research findings about authentication methods...",
    "priority": "high",
    "deadline": "2025-01-01T12:00:00Z",
    "context": {
      "domain": "security",
      "technology_stack": ["python", "fastapi", "jwt"]
    }
  },
  "priority": 1,
  "correlation_id": "workflow_abc123"
}
```

#### Broadcast Message
```http
POST /api/v1/agents/messages/broadcast
```

**Request Body:**
```json
{
  "from_agent": "project_manager",
  "type": "status_update",
  "payload": {
    "message": "Weekly standup meeting in 10 minutes",
    "meeting_link": "https://meet.example.com/room123"
  }
}
```

#### Get Agent Messages
```http
GET /api/v1/agents/{agent_id}/messages
```

### Agent Messaging with Model Selection

#### Send Message to Agent with Model Selection
```http
POST /api/v1/agents/{agent_id}/message
```

**Description:** Send a message to an agent and specify which LLM model to use for processing. This allows flexible model selection per request rather than being locked to a single model per agent.

**Request Body:**
```json
{
  "message": "What are the key principles of software architecture?",
  "model": "qwen3-0.6b",
  "temperature": 0.7,
  "max_tokens": 2048
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "research_assistant",
    "message": "What are the key principles of software architecture?",
    "model_used": "qwen3-0.6b",
    "success": true,
    "execution_time_ms": 1245.3,
    "response": "The key principles of software architecture include: 1. Modularity - Dividing the system into distinct components...",
    "tokens_generated": 156,
    "tokens_per_second": 8.2,
    "error_message": null
  }
}
```

**Parameters:**
- `message` (required): The text message to send to the agent
- `model` (optional): The specific LLM model to use (defaults to "qwen3-0.6b")
- `temperature` (optional): Controls randomness in the response (0.0-2.0, default: 0.7)
- `max_tokens` (optional): Maximum number of tokens to generate (default: 2048)

### Function Execution

#### Execute Function Synchronously
```http
POST /api/v1/agents/{agent_id}/functions/{function_name}
```

**Request Body:**
```json
{
  "parameters": {
    "query": "machine learning in healthcare",
    "k": 10,
    "score_threshold": 0.6,
    "collection_name": "medical_research"
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "success": true,
    "execution_time_ms": 1245.3,
    "agent_id": "research_assistant",
    "function": "retrieval",
    "result": {
      "documents_found": 8,
      "avg_score": 0.73,
      "collection": "medical_research"
    }
  }
}
```

#### Direct Agent Inference
```http
POST /api/v1/agents/{agent_id}/inference
```

**Request Body:**
```json
{
  "prompt": "What are the benefits of AI in medical diagnosis?",
  "max_tokens": 1024,
  "temperature": 0.3
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "research_assistant",
    "result": "AI in medical diagnosis offers several key benefits...",
    "execution_time_ms": 2156.7,
    "tokens_generated": 156,
    "tokens_per_second": 7.2,
    "engine_used": "qwen3-0.6b"
  }
}
```

#### List Agent Functions
```http
GET /api/v1/agents/{agent_id}/functions
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "research_assistant",
    "functions": [
      {
        "name": "inference",
        "description": "Generate text using the agent's LLM",
        "type": "builtin",
        "parameters": {
          "prompt": "The input text prompt",
          "max_tokens": "Maximum tokens to generate (optional)",
          "temperature": "Sampling temperature (optional)"
        }
      },
      {
        "name": "retrieval",
        "description": "Search documents using semantic similarity",
        "type": "builtin",
        "parameters": {
          "query": "The search query",
          "k": "Number of documents to retrieve (optional, default: 10)",
          "collection_name": "Collection to search in (optional)",
          "score_threshold": "Minimum similarity score (optional, default: 0.0)"
        }
      },
      {
        "name": "context_retrieval",
        "description": "Retrieve and format documents as context for AI responses",
        "type": "builtin",
        "parameters": {
          "query": "The search query for context retrieval",
          "k": "Number of documents to retrieve (optional, default: 5)",
          "context_format": "Format for context (summary or detailed)",
          "collection_name": "Collection name (optional)"
        }
      },
      {
        "name": "add_document",
        "description": "Add documents to the knowledge base",
        "type": "builtin",
        "parameters": {
          "documents": "Array of documents to add",
          "collection_name": "Collection name (optional)"
        }
      },
      {
        "name": "parse_pdf",
        "description": "Parse PDF files to extract text content",
        "type": "builtin",
        "parameters": {
          "pdf_data": "Base64 encoded PDF data",
          "method": "Parsing method (fast or comprehensive)",
          "auto_index": "Automatically index extracted content"
        }
      }
    ]
  }
}
```

### OpenAI Compatible Endpoints

#### Chat Completions (OpenAI Compatible)
```http
POST /v1/agents/{agent_id}/chat/completions
```

**Request Body (OpenAI Format):**
```json
{
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful assistant."
    },
    {
      "role": "user", 
      "content": "Hello, how are you?"
    }
  ],
  "model": "agent_model",
  "max_tokens": 150,
  "temperature": 0.7
}
```

**Response (OpenAI Format):**
```json
{
  "id": "chatcmpl-123",
  "object": "chat.completion",
  "created": 1677652288,
  "model": "agent_model",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Hello! I'm doing well, thank you for asking. How can I help you today?"
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 20,
    "completion_tokens": 25,
    "total_tokens": 45
  }
}
```

#### Agent Chat
```http
POST /v1/agents/{agent_id}/chat
```

#### Agent Message
```http
POST /v1/agents/{agent_id}/message
```

**Request Body:**
```json
{
  "message": "Hello, can you help me with a task?",
  "context": {
    "user_id": "user123",
    "conversation_id": "conv456"
  }
}
```

#### Agent Generate
```http
POST /v1/agents/{agent_id}/generate
```

#### Agent Respond
```http
POST /v1/agents/{agent_id}/respond
```

### Enhanced Document & Retrieval Management

#### Retrieve Documents with Advanced Filtering
```http
POST /retrieve
```

**Request Body:**
```json
{
  "query": "machine learning algorithms for medical diagnosis",
  "k": 15,
  "score_threshold": 0.7,
  "collection_name": "medical_research",
  "metadata_filter": {
    "category": "clinical_studies",
    "peer_reviewed": true,
    "publication_year": "2024"
  },
  "include_metadata": true,
  "rerank": true
}
```

**Response:**
```json
{
  "documents": [
    {
      "id": "doc_123",
      "text": "Machine learning algorithms have shown remarkable accuracy in medical diagnosis...",
      "score": 0.87,
      "metadata": {
        "source": "Clinical_AI_Study_2024.pdf",
        "category": "clinical_studies",
        "publication_year": "2024",
        "peer_reviewed": true,
        "institution": "Medical University"
      }
    }
  ],
  "query": "machine learning algorithms for medical diagnosis",
  "k": 15,
  "collection_name": "medical_research",
  "total_found": 12,
  "score_threshold": 0.7,
  "processing_time_ms": 145.2
}
```

#### Add Documents with Batch Processing
```http
POST /api/v1/documents
```

**Request Body:**
```json
{
  "documents": [
    {
      "text": "Artificial intelligence in healthcare has revolutionized medical diagnosis...",
      "metadata": {
        "source": "AI_Healthcare_Review_2024.pdf",
        "category": "review_article",
        "publication_date": "2024-06-15",
        "authors": ["Dr. Smith", "Dr. Johnson"],
        "keywords": ["AI", "healthcare", "diagnosis"]
      }
    },
    {
      "text": "Deep learning models for medical imaging have achieved state-of-the-art performance...",
      "metadata": {
        "source": "Deep_Learning_Medical_Imaging.pdf",
        "category": "research_paper",
        "publication_date": "2024-05-20"
      }
    }
  ],
  "collection_name": "medical_research",
  "batch_processing": true,
  "auto_chunk": true,
  "chunk_size": 512,
  "overlap": 50
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "processed_documents": 2,
    "total_chunks_created": 8,
    "collection_name": "medical_research",
    "processing_time_ms": 1234.5,
    "document_ids": ["doc_456", "doc_457"]
  }
}
```

#### Parse PDF with Enhanced Processing
```http
POST /parse-pdf
```

**Request Body:**
```json
{
  "pdf_data": "base64_encoded_pdf_content",
  "method": "comprehensive",
  "auto_index": true,
  "collection_name": "medical_research",
  "metadata": {
    "source": "research_paper.pdf",
    "category": "clinical_studies",
    "publication_date": "2024-06-01"
  },
  "processing_options": {
    "extract_tables": true,
    "extract_images": false,
    "preserve_formatting": true,
    "chunk_by_sections": true
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "document_id": "doc_789",
    "pages_processed": 15,
    "text_extracted": "Research findings indicate that AI-driven diagnostic tools...",
    "chunks_created": 23,
    "tables_extracted": 4,
    "processing_time_ms": 3456.7,
    "auto_indexed": true,
    "collection_name": "medical_research"
  }
}
```

#### Collection Management
```http
GET /api/v1/agents/collections
```

**Response:**
```json
{
  "success": true,
  "data": {
    "collections": [
      {
        "name": "medical_research",
        "document_count": 1247,
        "size_mb": 45.6,
        "created_date": "2024-01-15T10:30:00Z",
        "last_updated": "2024-06-24T14:22:00Z",
        "metadata_schema": {
          "category": "string",
          "publication_date": "string",
          "peer_reviewed": "boolean"
        }
      },
      {
        "name": "general_knowledge",
        "document_count": 892,
        "size_mb": 32.1,
        "created_date": "2024-02-01T09:15:00Z",
        "last_updated": "2024-06-23T16:45:00Z"
      }
    ],
    "total_collections": 2
  }
}
```

### System Management

#### Get System Status
```http
GET /api/v1/agents/system/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "system_running": true,
    "agent_count": 6,
    "system_status": "All systems operational",
    "agents": [
      {
        "id": "research_assistant",
        "name": "research_assistant",
        "running": true
      }
    ]
  }
}
```

#### Reload Configuration
```http
POST /api/v1/agents/system/reload
```

**Request Body:**
```json
{
  "config_file": "/path/to/config.yaml"
}
```

#### Get System Metrics
```http
GET /api/v1/agents/system/metrics
```

#### Get Orchestrator Status
```http
GET /api/v1/orchestration/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "status": "running",
    "is_running": true
  }
}
```

### Sequential Workflow Management

#### Create Sequential Workflow
```http
POST /api/v1/sequential/workflows
```

**Request Body:**
```json
{
  "name": "Medical Research Analysis Pipeline",
  "description": "Multi-step workflow for comprehensive medical research analysis",
  "steps": [
    {
      "step_id": "document_retrieval",
      "agent_id": "research_assistant",
      "function_name": "retrieval",
      "parameters": {
        "query": "{{global_context.research_topic}}",
        "k": 15,
        "score_threshold": 0.7,
        "collection_name": "medical_research"
      },
      "timeout_seconds": 60,
      "max_retries": 3
    },
    {
      "step_id": "context_synthesis",
      "agent_id": "knowledge_agent",
      "function_name": "context_retrieval",
      "parameters": {
        "query": "{{global_context.research_topic}} analysis",
        "k": 10,
        "context_format": "detailed"
      },
      "dependencies": ["document_retrieval"],
      "timeout_seconds": 45
    },
    {
      "step_id": "data_analysis",
      "agent_id": "data_analyst",
      "function_name": "inference",
      "parameters": {
        "prompt": "Analyze the research data and identify key trends",
        "context": "{{context_synthesis.output}}",
        "max_tokens": 2048,
        "temperature": 0.2
      },
      "dependencies": ["context_synthesis"],
      "timeout_seconds": 90
    }
  ],
  "global_context": {
    "research_topic": "AI in medical diagnosis",
    "output_format": "comprehensive_report"
  },
  "error_handling": {
    "retry_on_failure": true,
    "max_retries": 3,
    "continue_on_error": false
  }
}
```

#### Execute Sequential Workflow
```http
POST /api/v1/sequential/workflows/{workflow_id}/execute
```

**Request Body:**
```json
{
  "input_context": {
    "research_focus": "diagnostic_accuracy",
    "target_audience": "medical_professionals",
    "urgency": "standard"
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "execution_id": "exec_12345",
    "workflow_id": "workflow_abc123",
    "status": "running",
    "started_at": "2024-06-24T10:30:00Z",
    "estimated_completion": "2024-06-24T10:35:00Z"
  }
}
```

#### Get Workflow Status
```http
GET /api/v1/sequential/workflows/{workflow_id}/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "workflow_id": "workflow_abc123",
    "status": "completed",
    "progress": 1.0,
    "current_step": "data_analysis",
    "completed_steps": ["document_retrieval", "context_synthesis", "data_analysis"],
    "failed_steps": [],
    "start_time": "2024-06-24T10:30:00Z",
    "end_time": "2024-06-24T10:34:23Z",
    "total_execution_time_ms": 263000,
    "step_results": {
      "document_retrieval": {
        "status": "completed",
        "documents_found": 12,
        "execution_time_ms": 1456
      },
      "context_synthesis": {
        "status": "completed",
        "context_generated": true,
        "execution_time_ms": 2134
      },
      "data_analysis": {
        "status": "completed",
        "analysis_complete": true,
        "execution_time_ms": 5672
      }
    }
  }
}
```

## Collaboration Patterns

### Create Collaboration Group
```http
POST /api/v1/orchestration/collaboration-groups
```

**Request Body:**
```json
{
  "name": "Development Team",
  "pattern": "hierarchy",
  "agent_ids": ["project_manager", "code_assistant", "qa_specialist"],
  "consensus_threshold": 2
}
```

**Collaboration Patterns:**
- `sequential` - Agents work one after another
- `parallel` - Agents work simultaneously
- `pipeline` - Output of one agent feeds into the next
- `consensus` - Agents vote on the best result
- `hierarchy` - Master-slave coordination
- `negotiation` - Agents negotiate to reach agreement

### Execute Collaboration
```http
POST /api/v1/orchestration/collaboration-groups/{group_id}/execute
```

**Request Body:**
```json
{
  "task_description": "Develop a new feature for user authentication",
  "input_data": {
    "requirements": "Multi-factor authentication with biometric support",
    "deadline": "2024-01-15",
    "priority": "high"
  }
}
```

## Advanced Features

### Load Balancing

#### Select Optimal Agent
```http
POST /api/v1/orchestration/select-agent
```

**Request Body:**
```json
{
  "capability": "text_processing",
  "context": {
    "workload": "high",
    "complexity": "medium"
  }
}
```

#### Distribute Workload
```http
POST /api/v1/orchestration/distribute-workload
```

**Request Body:**
```json
{
  "task_type": "data_analysis",
  "tasks": [
    {"dataset": "sales_q1.csv", "analysis": "trend"},
    {"dataset": "sales_q2.csv", "analysis": "trend"},
    {"dataset": "sales_q3.csv", "analysis": "trend"}
  ]
}
```

### Monitoring

#### Get Orchestration Metrics
```http
GET /api/v1/orchestration/metrics
```

**Response:**
```json
{
  "success": true,
  "data": {
    "active_workflows": 3,
    "completed_workflows": 15,
    "failed_workflows": 1,
    "total_workflows": 19,
    "collaboration_groups": 2,
    "orchestrator_status": "running",
    "workflow_execution_times": {
      "average_ms": 2340,
      "min_ms": 450,
      "max_ms": 8920
    },
    "resource_usage": {
      "cpu_percent": 45.2,
      "memory_mb": 1024,
      "active_threads": 12
    }
  }
}
```

#### Get Agent System Metrics
```http
GET /api/v1/agents/system/metrics
```

**Response:**
```json
{
  "success": true,
  "data": {
    "total_agents": 6,
    "running_agents": 5,
    "stopped_agents": 1,
    "total_functions_executed": 1247,
    "average_execution_time_ms": 856.3,
    "active_jobs": 4,
    "completed_jobs": 189,
    "failed_jobs": 3,
    "system_uptime_seconds": 86400,
    "memory_usage_mb": 512,
    "cpu_usage_percent": 23.5
  }
}
```

## Configuration

**Important Note**: The system now supports flexible model selection. The `model_name` field in agent configurations is optional, and models can be specified per request when interacting with agents.

### Agent Configuration (YAML)

```yaml
# System configuration
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
    max_connections: 20

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
  - name: parse_pdf
    type: builtin
    description: Parse PDF files to extract text content
    async_capable: true
    timeout_ms: 120000
  - name: get_embedding
    type: builtin
    description: Generate embedding vectors for text content
    async_capable: true
    timeout_ms: 30000

# Agent definitions
agents:
  - name: research_specialist
    type: research
    role: Information researcher and analyzer
    system_prompt: "You are a research assistant specialized in information gathering, analysis, and synthesis."
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
      # model_name is now optional - specify at request time
      api_endpoint: http://localhost:8080/v1
      instruction: You are an expert research assistant.
      temperature: 0.3
      max_tokens: 2048
      timeout_seconds: 60
      max_retries: 3
    auto_start: true
    max_concurrent_jobs: 4
    heartbeat_interval_seconds: 10
    custom_settings:
      enable_logging: true
      fact_checking: enabled
      search_depth: comprehensive
      rag_enabled: true
    metadata:
      created_by: system
      version: "2.0"
      tags: ["research", "rag", "documents"]

  - name: document_manager
    type: document_management
    role: Document processing and knowledge base management specialist
    system_prompt: "You are a document management specialist who helps with organizing, processing, and managing knowledge bases."
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
      instruction: You are a document management specialist.
      temperature: 0.2
      max_tokens: 2048
      timeout_seconds: 120
      max_retries: 3
    auto_start: true
    max_concurrent_jobs: 5
    heartbeat_interval_seconds: 15
    custom_settings:
      auto_chunking: true
      batch_processing: true
      default_collection: documents
      quality_validation: enabled
```

### Function Configuration

```yaml
functions:
  - name: "advanced_text_processing"
    type: "builtin"
    description: "Advanced text processing with sentiment analysis and entity extraction"
    parameters:
      text: "Input text to process"
      operation: "Type of operation (analyze, summarize, extract)"
      language: "Language code (optional)"
    timeout_ms: 15000
    retry_count: 2
    
  - name: "web_research"
    type: "external_api"
    description: "Web search and content extraction"
    api_endpoint: "https://api.search.example.com/v1"
    parameters:
      query: "Search query"
      limit: "Number of results (default: 10)"
      domain_filter: "Domain restrictions (optional)"
    timeout_ms: 30000
    
  - name: "custom_analysis"
    type: "llm"
    description: "Custom LLM-based analysis function"
    prompt_template: "Analyze the following data and provide insights: {input_data}"
    parameters:
      input_data: "Data to analyze"
      analysis_type: "Type of analysis required"
    model_override:
      temperature: 0.3
      max_tokens: 1500
      
  - name: "retrieval"
    type: "builtin"
    description: "Search and retrieve relevant documents from knowledge base"
    parameters:
      query: "Search query for document retrieval"
      k: "Number of documents to retrieve (default: 5)"
      score_threshold: "Minimum similarity score (default: 0.0)"
      collection_name: "Collection name (optional)"
    timeout_ms: 60000
    
  - name: "context_retrieval"
    type: "builtin"
    description: "Retrieve and format documents as context for enhanced responses"
    parameters:
      query: "Search query for context retrieval"
      k: "Number of documents to retrieve (default: 3)"
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
```

## API Versioning

The Kolosal Server Agent System API supports multiple versions:

- **v1**: Current stable version (`/api/v1/` or `/v1/`)
- **Legacy**: Some endpoints support legacy paths without version prefix

All new applications should use the `/api/v1/` prefix for consistency.

## WebSocket Support

For real-time updates and streaming responses, the system supports WebSocket connections:

```javascript
// Connect to agent message stream
const ws = new WebSocket('ws://localhost:8080/ws/agents/messages');

ws.onmessage = function(event) {
  const message = JSON.parse(event.data);
  console.log('Received:', message);
};

// Connect to workflow status updates
const workflowWs = new WebSocket('ws://localhost:8080/ws/orchestration/status');
```

## Error Handling

All API endpoints return standardized error responses:

```json
{
  "success": false,
  "error": "Agent not found",
  "code": 404
}
```

Common error codes:
- `400` - Bad Request (invalid input)
- `404` - Not Found (agent/workflow not found)
- `500` - Internal Server Error
- `503` - Service Unavailable (agent system down)

## Rate Limiting

- Agent function calls: 100 requests per minute per agent
- Workflow executions: 10 concurrent workflows per user
- Message broadcasts: 50 messages per minute per agent

## Security

- All API endpoints require authentication
- Agent-to-agent communication is encrypted
- Function execution is sandboxed
- Input validation and sanitization applied
- Rate limiting and abuse prevention

## Examples

### Complete Agent Lifecycle Example with RAG

```bash
# 1. Create a RAG-enabled research agent
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "rag_researcher",
    "type": "research",
    "role": "RAG-enhanced Research Assistant",
    "system_prompt": "You are a research assistant that uses document retrieval to provide accurate, context-rich responses.",
    "capabilities": ["document_retrieval", "context_retrieval", "text_processing", "document_management"],
    "functions": ["inference", "retrieval", "context_retrieval", "add_document", "parse_pdf"],
    "auto_start": true,
    "max_concurrent_jobs": 4
  }'

# 2. Verify agent is running and get capabilities
curl http://localhost:8080/api/v1/agents/rag_researcher

# 3. Add documents to knowledge base
curl -X POST http://localhost:8080/api/v1/agents/rag_researcher/functions/add_document \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {
      "documents": [
        {
          "text": "Machine learning algorithms have revolutionized medical diagnosis...",
          "metadata": {
            "source": "Medical_AI_2024.pdf",
            "category": "healthcare_ai",
            "publication_date": "2024-06-01"
          }
        }
      ],
      "collection_name": "medical_research"
    }
  }'

# 4. Perform document retrieval
curl -X POST http://localhost:8080/api/v1/agents/rag_researcher/functions/retrieval \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {
      "query": "machine learning medical diagnosis accuracy",
      "k": 10,
      "score_threshold": 0.6,
      "collection_name": "medical_research"
    }
  }'

# 5. Perform RAG-enhanced inference
curl -X POST http://localhost:8080/api/v1/agents/rag_researcher/inference \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "What are the latest developments in AI for medical diagnosis?",
    "max_tokens": 1024,
    "temperature": 0.3
  }'

# 6. Parse and index PDF document
curl -X POST http://localhost:8080/parse-pdf \
  -H "Content-Type: application/json" \
  -d '{
    "pdf_data": "base64_encoded_pdf_content",
    "method": "comprehensive",
    "auto_index": true,
    "collection_name": "medical_research",
    "metadata": {
      "source": "clinical_study.pdf",
      "category": "clinical_research"
    }
  }'
```

### Advanced Multi-Agent Workflow with RAG

```bash
# 1. Create multiple specialized agents
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "document_processor",
    "type": "document_management",
    "capabilities": ["document_management", "document_parsing"],
    "functions": ["add_document", "parse_pdf", "parse_docx", "get_embedding"]
  }'

curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "knowledge_synthesizer",
    "type": "research",
    "capabilities": ["context_retrieval", "knowledge_synthesis"],
    "functions": ["inference", "context_retrieval", "retrieval"]
  }'

# 2. Create comprehensive research workflow
curl -X POST http://localhost:8080/api/v1/sequential/workflows \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Comprehensive Medical Research Pipeline",
    "description": "End-to-end research workflow with document processing and analysis",
    "steps": [
      {
        "step_id": "document_processing",
        "agent_id": "document_processor",
        "function_name": "parse_pdf",
        "parameters": {
          "pdf_data": "{{input.pdf_data}}",
          "method": "comprehensive",
          "auto_index": true,
          "collection_name": "research_papers"
        }
      },
      {
        "step_id": "knowledge_retrieval",
        "agent_id": "rag_researcher",
        "function_name": "retrieval",
        "parameters": {
          "query": "{{input.research_query}}",
          "k": 15,
          "score_threshold": 0.7,
          "collection_name": "research_papers"
        },
        "dependencies": ["document_processing"]
      },
      {
        "step_id": "context_synthesis",
        "agent_id": "knowledge_synthesizer",
        "function_name": "context_retrieval",
        "parameters": {
          "query": "{{input.research_query}} comprehensive analysis",
          "k": 10,
          "context_format": "detailed"
        },
        "dependencies": ["knowledge_retrieval"]
      },
      {
        "step_id": "research_analysis",
        "agent_id": "data_analyst",
        "function_name": "inference",
        "parameters": {
          "prompt": "Analyze the research findings and provide comprehensive insights",
          "context": "{{context_synthesis.output}}",
          "max_tokens": 2048,
          "temperature": 0.2
        },
        "dependencies": ["context_synthesis"]
      }
    ],
    "global_context": {
      "research_domain": "medical_ai",
      "quality_standards": "high",
      "output_format": "detailed_report"
    }
  }'

# 3. Execute the workflow
curl -X POST http://localhost:8080/api/v1/sequential/workflows/workflow_123/execute \
  -H "Content-Type: application/json" \
  -d '{
    "input_context": {
      "pdf_data": "base64_encoded_research_paper",
      "research_query": "AI diagnostic accuracy in radiology",
      "target_audience": "medical_researchers"
    }
  }'

# 4. Monitor workflow progress
curl http://localhost:8080/api/v1/sequential/workflows/workflow_123/status
```

### Document Management and Collection Operations

```bash
# 1. Create specialized document collection
curl -X POST http://localhost:8080/api/v1/agents/collections \
  -H "Content-Type: application/json" \
  -d '{
    "collection_name": "clinical_trials",
    "description": "Clinical trial studies and research papers",
    "metadata_schema": {
      "study_phase": "string",
      "intervention_type": "string",
      "primary_outcome": "string",
      "enrollment": "integer",
      "completion_date": "string"
    }
  }'

# 2. Bulk document upload with metadata
curl -X POST http://localhost:8080/api/v1/documents \
  -H "Content-Type: application/json" \
  -d '{
    "documents": [
      {
        "text": "Phase III randomized controlled trial of AI-assisted diagnosis...",
        "metadata": {
          "study_phase": "Phase III",
          "intervention_type": "AI_diagnostic_tool",
          "primary_outcome": "diagnostic_accuracy",
          "enrollment": 2500,
          "completion_date": "2024-03-15"
        }
      }
    ],
    "collection_name": "clinical_trials",
    "batch_processing": true,
    "auto_chunk": true
  }'

# 3. Advanced semantic search with filtering
curl -X POST http://localhost:8080/retrieve \
  -H "Content-Type: application/json" \
  -d '{
    "query": "AI diagnostic accuracy randomized trial",
    "k": 20,
    "score_threshold": 0.75,
    "collection_name": "clinical_trials",
    "metadata_filter": {
      "study_phase": "Phase III",
      "intervention_type": "AI_diagnostic_tool"
    },
    "include_metadata": true,
    "rerank": true
  }'

# 4. Test document service connectivity
curl -X POST http://localhost:8080/api/v1/agents/document_processor/functions/test_document_service \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {}
  }'
```

### Multi-Agent Workflow Example

```bash
# 1. Create multiple agents
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "researcher",
    "type": "research",
    "capabilities": ["web_search", "text_processing"]
  }'

curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "writer",
    "type": "creative",
    "capabilities": ["text_processing", "content_creation"]
  }'

curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "reviewer",
    "type": "quality_assurance",
    "capabilities": ["text_processing", "quality_review"]
  }'

# 2. Create collaborative workflow
curl -X POST http://localhost:8080/api/v1/orchestration/workflows \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Content Creation Pipeline",
    "description": "Research, write, and review content workflow",
    "global_context": {
      "topic": "Artificial Intelligence in Healthcare",
      "target_audience": "medical professionals",
      "word_count": 1500,
      "tone": "professional"
    },
    "steps": [
      {
        "step_id": "research_phase",
        "agent_id": "researcher",
        "function_name": "web_search",
        "parameters": {
          "query": "AI healthcare applications 2025",
          "limit": 15
        },
        "dependencies": [],
        "parallel_allowed": true
      },
      {
        "step_id": "content_creation",
        "agent_id": "writer",
        "function_name": "text_processing",
        "parameters": {
          "operation": "write_article",
          "context": "{{research_phase.result}}"
        },
        "dependencies": ["research_phase"],
        "parallel_allowed": false
      },
      {
        "step_id": "quality_review",
        "agent_id": "reviewer",
        "function_name": "text_processing",
        "parameters": {
          "operation": "quality_review",
          "content": "{{content_creation.result}}"
        },
        "dependencies": ["content_creation"],
        "parallel_allowed": false
      }
    ]
  }'

# 3. Execute workflow
curl -X POST http://localhost:8080/api/v1/orchestration/workflows/workflow_123/execute-async \
  -H "Content-Type: application/json" \
  -d '{
    "input_context": {
      "domain": "healthcare",
      "urgency": "high",
      "target_publication": "Medical Journal"
    }
  }'

# 4. Monitor workflow progress
curl http://localhost:8080/api/v1/orchestration/workflows/workflow_123/status

# 5. Get final results
curl http://localhost:8080/api/v1/orchestration/workflows/workflow_123/result
```

### RAG-Enhanced Agent Example

```bash
# 1. Create RAG-capable agent
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "rag_researcher",
    "type": "research",
    "capabilities": ["retrieval", "context_retrieval", "text_processing"],
    "functions": ["inference", "retrieval", "context_retrieval"],
    "auto_start": true
  }'

# 2. Add documents to knowledge base
curl -X POST http://localhost:8080/api/v1/documents \
  -H "Content-Type: application/json" \
  -d '{
    "documents": [
      {
        "text": "Machine learning is a subset of artificial intelligence that enables computers to learn and improve from experience without being explicitly programmed.",
        "metadata": {
          "source": "ML_Guide.pdf",
          "page": 1,
          "category": "introduction"
        }
      }
    ]
  }'

# 3. Parse and index PDF document
curl -X POST http://localhost:8080/parse-pdf \
  -H "Content-Type: application/json" \
  -d '{
    "pdf_data": "base64_encoded_pdf_content",
    "method": "fast",
    "auto_index": true
  }'

# 4. Retrieve relevant documents
curl -X POST http://localhost:8080/retrieve \
  -H "Content-Type: application/json" \
  -d '{
    "query": "what is machine learning",
    "k": 3,
    "score_threshold": 0.5
  }'

# 5. Use agent with retrieval function
curl -X POST http://localhost:8080/api/v1/agents/rag_researcher/execute \
  -H "Content-Type: application/json" \
  -d '{
    "function": "context_retrieval",
    "parameters": {
      "query": "machine learning applications",
      "k": 5,
      "context_format": "detailed"
    }
  }'

# 6. Create RAG workflow
curl -X POST http://localhost:8080/api/v1/orchestration/workflows \
  -H "Content-Type: application/json" \
  -d '{
    "name": "RAG Research Pipeline",
    "description": "Retrieve context and generate research report",
    "steps": [
      {
        "step_id": "retrieve_context",
        "agent_id": "rag_researcher",
        "function_name": "context_retrieval",
        "parameters": {
          "query": "AI healthcare applications",
          "k": 10,
          "context_format": "detailed"
        }
      },
      {
        "step_id": "generate_report",
        "agent_id": "rag_researcher",
        "function_name": "inference",
        "parameters": {
          "prompt": "Based on the retrieved context, write a comprehensive research report about AI in healthcare",
          "context": "{{retrieve_context.result}}"
        },
        "dependencies": ["retrieve_context"]
      }
    ]
  }'
```

### OpenAI Compatible Usage Example

```bash
# Using agent as OpenAI-compatible endpoint
curl -X POST http://localhost:8080/v1/agents/research_bot/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {
        "role": "system",
        "content": "You are a helpful research assistant."
      },
      {
        "role": "user",
        "content": "Can you help me find recent developments in quantum computing?"
      }
    ],
    "max_tokens": 500,
    "temperature": 0.7
  }'
```

### Agent Communication Example

```bash
# Send message between agents
curl -X POST http://localhost:8080/api/v1/agents/messages/send \
  -H "Content-Type: application/json" \
  -d '{
    "from_agent": "researcher",
    "to_agent": "writer",
    "type": "task_request",
    "payload": {
      "task": "Create article based on research findings",
      "data": {
        "research_summary": "Key findings about AI in healthcare...",
        "sources": ["source1.com", "source2.com"],
        "deadline": "2025-06-25T12:00:00Z"
      },
      "priority": "high"
    },
    "correlation_id": "workflow_abc123"
  }'

# Broadcast system-wide message
curl -X POST http://localhost:8080/api/v1/agents/messages/broadcast \
  -H "Content-Type: application/json" \
  -d '{
    "from_agent": "system_manager",
    "type": "system_announcement",
    "payload": {
      "message": "System maintenance scheduled for 2025-06-25 at 02:00 UTC",
      "affected_services": ["orchestration", "external_apis"],
      "duration_minutes": 30
    }
  }'
```

### System Monitoring Example

```bash
# Get comprehensive system status
curl http://localhost:8080/api/v1/agents/system/status

# Get detailed metrics
curl http://localhost:8080/api/v1/agents/system/metrics

# Get orchestration metrics
curl http://localhost:8080/api/v1/orchestration/metrics

# Get orchestrator status
curl http://localhost:8080/api/v1/orchestration/status
```

This comprehensive agent system provides powerful capabilities for building intelligent, collaborative applications with autonomous agents.

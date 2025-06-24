# Agent System API Documentation

## Overview

The Kolosal Server Agent System provides a comprehensive multi-agent framework that enables:
- Dynamic agent creation and management with UUID-based identification
- Inter-agent communication and coordination with message correlation
- Workflow orchestration and automation with dependency management
- Collaborative problem-solving patterns with consensus mechanisms
- Load balancing and optimization with automatic resource management
- Real-time monitoring and performance metrics
- Hot-reloading configuration without system restart

## Architecture

### Core Components

1. **AgentCore** - Individual agent implementation with thread-safe execution and UUID identification
2. **YAMLConfigurableAgentManager** - System-wide agent management with hot-reloading support
3. **AgentOrchestrator** - Advanced workflow and collaboration orchestration with dependency resolution
4. **MessageRouter** - Inter-agent communication with priority queuing and correlation tracking
5. **FunctionManager** - Function execution management supporting builtin, LLM, external API, and custom functions
6. **JobManager** - Asynchronous job handling with priority queues and status tracking
7. **EventSystem** - Real-time event processing and notification system
8. **ConfigurableAgentFactory** - Factory for creating agents from YAML configurations

### Agent Types

The system supports 6 predefined agent types with specialized capabilities:

- **Research Agents** (`research`) - Information gathering, web search, data analysis, and synthesis
- **Development Agents** (`development`) - Code generation, review, debugging, and optimization
- **Analytics Agents** (`analytics`) - Data analysis, statistical processing, and visualization
- **Creative Agents** (`creative`) - Content creation, writing, copywriting, and creative tasks
- **Management Agents** (`management`) - Project coordination, task management, and progress tracking
- **QA Agents** (`quality_assurance`) - Quality assurance, testing, validation, and process improvement

### Function Types

- **Builtin Functions** - Native server functions (inference, text_processing, data_analysis)
- **LLM Functions** - AI-powered functions using language models with configurable prompts
- **External API Functions** - Integration with external services (web_search, etc.)
- **Custom Functions** - User-defined function implementations

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
      "uuid": "agent_12345_67890",
      "id": "research_assistant",
      "name": "research_assistant",
      "type": "research",
      "capabilities": [
        "web_search", 
        "text_processing", 
        "data_analysis",
        "information_synthesis"
      ],
      "running": true
    },
    {
      "uuid": "agent_23456_78901",
      "id": "code_assistant",
      "name": "code_assistant", 
      "type": "development",
      "capabilities": [
        "code_generation",
        "code_review", 
        "debugging",
        "optimization"
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
    "uuid": "agent_12345_67890",
    "id": "research_assistant",
    "name": "research_assistant",
    "type": "research",
    "role": "AI Research Assistant",
    "system_prompt": "You are a research assistant specialized in information gathering and analysis.",
    "capabilities": [
      "web_search",
      "text_processing", 
      "data_analysis",
      "information_synthesis"
    ],
    "running": true,
    "functions": [
      "inference",
      "web_search", 
      "text_processing",
      "data_analysis"
    ],
    "max_concurrent_jobs": 3,
    "heartbeat_interval_seconds": 10,
    "llm_config": {
      "model_name": "test-qwen-0.6b",
      "api_endpoint": "http://localhost:8080/v1",
      "temperature": 0.7,
      "max_tokens": 2048
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
  "name": "custom_agent",
  "type": "generic",
  "role": "Custom processing agent",
  "system_prompt": "You are a helpful assistant specialized in custom tasks.",
  "capabilities": [
    "text_processing",
    "data_analysis",
    "inference"
  ],
  "functions": [
    "inference",
    "text_processing",
    "data_analysis"
  ],
  "llm_config": {
    "model_name": "test-qwen-0.6b",
    "api_endpoint": "http://localhost:8080/v1",
    "instruction": "You are a helpful assistant specialized in custom tasks.",
    "temperature": 0.7,
    "max_tokens": 2048,
    "timeout_seconds": 60,
    "max_retries": 3,
    "stream": false
  },
  "auto_start": true,
  "max_concurrent_jobs": 3,
  "heartbeat_interval_seconds": 10,
  "custom_settings": {
    "enable_logging": true,
    "priority_level": "normal",
    "workflow_enabled": true,
    "collaboration_enabled": true
  },
  "metadata": {
    "created_by": "user123",
    "version": "1.0",
    "tags": ["custom", "processing", "experimental"]
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "agent_id": "custom_agent_12345",
    "status": "created"
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

### Function Execution

#### Execute Function Synchronously
```http
POST /api/v1/agents/{agent_id}/execute
```

**Request Body:**
```json
{
  "function": "text_processing",
  "parameters": {
    "text": "The quick brown fox jumps over the lazy dog",
    "operation": "analyze"
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "success": true,
    "execution_time_ms": 234.5,
    "result": {
      "word_count": 9,
      "character_count": 43,
      "sentiment": "neutral",
      "readability_score": 8.2
    }
  }
}
```

#### Execute Function Asynchronously
```http
POST /api/v1/agents/{agent_id}/execute-async
```

**Request Body:**
```json
{
  "function": "code_generation",
  "parameters": {
    "requirements": "Create a REST API endpoint for user authentication",
    "language": "python",
    "framework": "fastapi"
  },
  "priority": 1
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "job_id": "job_67890",
    "status": "queued",
    "agent_id": "code_assistant",
    "function": "code_generation"
  }
}
```

#### Get Job Status
```http
GET /api/v1/agents/jobs/{job_id}/status
```

**Response:**
```json
{
  "success": true,
  "data": {
    "job_id": "job_67890",
    "status": "running",
    "agent_id": "code_assistant",
    "function": "code_generation",
    "priority": 1,
    "created_at": "2025-06-24T10:30:00Z",
    "started_at": "2025-06-24T10:30:05Z",
    "progress": 0.65,
    "estimated_completion": "2025-06-24T10:32:00Z"
  }
}
```

#### Get Job Result
```http
GET /api/v1/agents/jobs/{job_id}/result
```

**Response:**
```json
{
  "success": true,
  "data": {
    "job_id": "job_67890",
    "status": "completed",
    "agent_id": "code_assistant",
    "function": "code_generation",
    "result": {
      "generated_code": "from fastapi import FastAPI, HTTPException\n\napp = FastAPI()\n\n@app.post('/auth/login')\ndef login(credentials: dict):...",
      "language": "python",
      "framework": "fastapi",
      "lines_of_code": 45,
      "complexity_score": 3.2
    },
    "execution_time_ms": 1847,
    "completed_at": "2025-06-24T10:31:52Z"
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
  "config_file": "/path/to/agents.yaml"
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

## Workflow Orchestration

### Create Workflow
```http
POST /api/v1/orchestration/workflows
```

**Request Body:**
```json
{
  "name": "Content Creation Workflow",
  "description": "Research, write, and review content",
  "global_context": {
    "topic": "AI in Healthcare",
    "target_audience": "medical professionals",
    "word_count": 2000
  },
  "steps": [
    {
      "step_id": "research",
      "agent_id": "research_assistant",
      "function_name": "web_search",
      "parameters": {
        "query": "AI applications in healthcare 2024",
        "limit": 10
      },
      "dependencies": [],
      "parallel_allowed": true
    },
    {
      "step_id": "write_content",
      "agent_id": "content_creator",
      "function_name": "text_processing",
      "parameters": {
        "operation": "write_article"
      },
      "dependencies": ["research"],
      "parallel_allowed": false
    },
    {
      "step_id": "review_content",
      "agent_id": "qa_specialist",
      "function_name": "text_processing",
      "parameters": {
        "operation": "quality_review"
      },
      "dependencies": ["write_content"],
      "parallel_allowed": false
    }
  ]
}
```

### Execute Workflow
```http
POST /api/v1/orchestration/workflows/{workflow_id}/execute
```

### Execute Workflow Asynchronously
```http
POST /api/v1/orchestration/workflows/{workflow_id}/execute-async
```

### Get Workflow Result
```http
GET /api/v1/orchestration/workflows/{workflow_id}/result
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

### Agent Configuration (YAML)

```yaml
agents:
  - name: "research_specialist"
    type: "research"
    role: "AI Research Specialist"
    system_prompt: "You are an expert research assistant specialized in information gathering, analysis, and synthesis."
    capabilities:
      - "web_search"
      - "text_processing"
      - "data_analysis"
      - "information_synthesis"
    functions:
      - "inference"
      - "web_search"
      - "text_processing"
      - "data_analysis"
    llm_config:
      model_name: "test-qwen-0.6b"
      api_endpoint: "http://localhost:8080/v1"
      instruction: "You are an expert research assistant."
      temperature: 0.7
      max_tokens: 2048
      timeout_seconds: 60
      max_retries: 3
      stream: false
    auto_start: true
    max_concurrent_jobs: 3
    heartbeat_interval_seconds: 10
    custom_settings:
      enable_logging: true
      priority_level: "high"
      workflow_enabled: true
      collaboration_enabled: true
    metadata:
      created_by: "system"
      version: "1.2"
      tags: ["research", "analysis", "primary"]

  - name: "code_generator"
    type: "development"
    role: "Code Generation Specialist"
    system_prompt: "You are a software development expert specialized in generating high-quality, production-ready code."
    capabilities:
      - "code_generation"
      - "code_review"
      - "debugging"
      - "optimization"
    functions:
      - "inference"
      - "text_processing"
    auto_start: false
    max_concurrent_jobs: 2
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

### Complete Agent Lifecycle Example

```bash
# 1. Create a research agent
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "research_bot",
    "type": "research",
    "role": "Research Assistant",
    "system_prompt": "You are a helpful research assistant.",
    "capabilities": ["web_search", "text_processing", "data_analysis"],
    "functions": ["inference", "web_search", "text_processing"],
    "auto_start": true,
    "max_concurrent_jobs": 3
  }'

# 2. Verify agent is running
curl http://localhost:8080/api/v1/agents/research_bot

# 3. Execute a research task
curl -X POST http://localhost:8080/api/v1/agents/research_bot/execute-async \
  -H "Content-Type: application/json" \
  -d '{
    "function": "web_search",
    "parameters": {
      "query": "machine learning trends 2025",
      "limit": 10
    },
    "priority": 1
  }'

# 4. Check job status (using job_id from step 3 response)
curl http://localhost:8080/api/v1/agents/jobs/job_12345/status

# 5. Get job results
curl http://localhost:8080/api/v1/agents/jobs/job_12345/result
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

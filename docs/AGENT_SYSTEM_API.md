# Agent System API Documentation

## Overview

The Kolosal Server Agent System provides a comprehensive multi-agent framework that enables:
- Dynamic agent creation and management
- Inter-agent communication and coordination
- Workflow orchestration and automation
- Collaborative problem-solving patterns
- Load balancing and optimization

## Architecture

### Core Components

1. **AgentCore** - Individual agent implementation
2. **YAMLConfigurableAgentManager** - System-wide agent management
3. **AgentOrchestrator** - Workflow and collaboration orchestration
4. **MessageRouter** - Inter-agent communication
5. **FunctionManager** - Function execution management
6. **JobManager** - Asynchronous job handling

### Agent Types

- **Research Agents** - Information gathering and analysis
- **Development Agents** - Code generation and review
- **Analytics Agents** - Data analysis and visualization
- **Creative Agents** - Content creation and writing
- **Management Agents** - Project coordination
- **QA Agents** - Quality assurance and testing

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
      "id": "research_assistant",
      "name": "research_assistant",
      "type": "research",
      "capabilities": ["web_search", "text_processing", "data_analysis"],
      "running": true
    }
  ],
  "count": 1
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
    "id": "research_assistant",
    "name": "research_assistant",
    "type": "research",
    "capabilities": ["web_search", "text_processing", "data_analysis"],
    "running": true
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
  "capabilities": ["text_processing"],
  "functions": ["text_processing"],
  "auto_start": true,
  "max_concurrent_jobs": 3
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

#### Stop Agent
```http
POST /api/v1/agents/{agent_id}/stop
```

#### Delete Agent
```http
DELETE /api/v1/agents/{agent_id}
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
    "task": "Generate code based on research",
    "data": "Research findings...",
    "priority": "high"
  }
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

#### Get Job Result
```http
GET /api/v1/agents/jobs/{job_id}/result
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
    "orchestrator_status": "running"
  }
}
```

## Configuration

### Agent Configuration (YAML)

```yaml
agents:
  - name: "custom_agent"
    type: "generic"
    role: "Custom processing agent"
    system_prompt: "You are a helpful assistant."
    capabilities:
      - "text_processing"
      - "data_analysis"
    functions:
      - "text_processing"
    llm_config:
      model_name: "gpt-4"
      temperature: 0.7
      max_tokens: 2048
    auto_start: true
    max_concurrent_jobs: 3
```

### Function Configuration

```yaml
functions:
  - name: "custom_function"
    type: "builtin"
    description: "Custom processing function"
    parameters:
      input: "Input data"
      operation: "Type of operation"
    timeout_ms: 10000
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

### Complete Workflow Example

```bash
# 1. Create agents
curl -X POST http://localhost:8080/api/v1/agents \
  -H "Content-Type: application/json" \
  -d '{
    "name": "research_bot",
    "type": "research",
    "capabilities": ["web_search", "text_processing"]
  }'

# 2. Create workflow
curl -X POST http://localhost:8080/api/v1/orchestration/workflows \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Research Pipeline",
    "steps": [
      {
        "step_id": "search",
        "agent_id": "research_bot",
        "function_name": "web_search",
        "parameters": {"query": "machine learning trends"}
      }
    ]
  }'

# 3. Execute workflow
curl -X POST http://localhost:8080/api/v1/orchestration/workflows/workflow_123/execute \
  -H "Content-Type: application/json" \
  -d '{"input_context": {"domain": "technology"}}'

# 4. Get results
curl http://localhost:8080/api/v1/orchestration/workflows/workflow_123/result
```

This comprehensive agent system provides powerful capabilities for building intelligent, collaborative applications with autonomous agents.

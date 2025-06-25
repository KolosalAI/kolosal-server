# Sequential Workflow System

## Overview

The Sequential Workflow System is an enhanced workflow orchestration component of the Kolosal Server that enables step-by-step execution of agent tasks in a well-defined sequence. This system is designed for complex multi-agent scenarios where tasks need to be executed in a specific order, with proper error handling, monitoring, and result aggregation.

## Key Features

### 1. **Step-by-Step Execution**
- **Sequential Processing**: Execute agents one after another in a defined order
- **Context Propagation**: Pass results from one step to the next seamlessly
- **Conditional Execution**: Skip steps based on conditions or previous results
- **Dependency Management**: Ensure proper execution order with built-in validation

### 2. **Advanced Error Handling**
- **Retry Mechanisms**: Automatic retry with exponential backoff
- **Failure Strategies**: Continue or stop on failure per step or globally
- **Error Isolation**: Isolate failures to prevent cascade effects
- **Graceful Degradation**: Handle partial workflow completion

### 3. **Comprehensive Monitoring**
- **Real-time Status**: Track workflow progress in real-time
- **Execution Metrics**: Detailed timing and performance metrics per step
- **Result Storage**: Persistent storage of intermediate and final results
- **Audit Trail**: Complete audit trail of workflow execution

### 4. **Flexible Configuration**
- **YAML Configuration**: Easy workflow definition using YAML
- **JSON API**: Dynamic workflow creation via REST API
- **Template System**: Reusable workflow templates
- **Parameter Injection**: Dynamic parameter injection at runtime

## Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                Sequential Workflow System                   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌──────────────────┐  ┌─────────────┐ │
│  │ Workflow        │  │ Step Executor    │  │ Result      │ │
│  │ Builder         │  │                  │  │ Manager     │ │
│  └─────────────────┘  └──────────────────┘  └─────────────┘ │
│  ┌─────────────────┐  ┌──────────────────┐  ┌─────────────┐ │
│  │ Configuration   │  │ Error Handler    │  │ Monitor     │ │
│  │ Manager         │  │                  │  │ System      │ │
│  └─────────────────┘  └──────────────────┘  └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    Agent Management Layer                   │
│  ┌─────────────────┐  ┌──────────────────┐  ┌─────────────┐ │
│  │ Research        │  │ Code Assistant   │  │ Data        │ │
│  │ Assistant       │  │                  │  │ Analyst     │ │
│  └─────────────────┘  └──────────────────┘  └─────────────┘ │
│  ┌─────────────────┐  ┌──────────────────┐  ┌─────────────┐ │
│  │ Content         │  │ Project Manager  │  │ QA          │ │
│  │ Creator         │  │                  │  │ Specialist  │ │
│  └─────────────────┘  └──────────────────┘  └─────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Input Context
     │
     ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Step 1     │───▶│   Step 2     │───▶│   Step 3     │
│  Research    │    │   Writing    │    │   Review     │
└──────────────┘    └──────────────┘    └──────────────┘
     │                    │                    │
     ▼                    ▼                    ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Step Result  │    │ Step Result  │    │ Step Result  │
│   + Context  │    │   + Context  │    │   + Context  │
└──────────────┘    └──────────────┘    └──────────────┘
                                             │
                                             ▼
                                    ┌──────────────┐
                                    │ Final Result │
                                    └──────────────┘
```

## API Reference

### Workflow Management

#### Create Workflow
```http
POST /api/v1/sequential-workflows
Content-Type: application/json

{
  "workflow_id": "content_creation_workflow",
  "workflow_name": "Content Creation Pipeline",
  "description": "Research, write, and review content",
  "stop_on_failure": true,
  "max_execution_time_seconds": 300,
  "global_context": {
    "topic": "AI in Healthcare",
    "audience": "professionals"
  },
  "steps": [
    {
      "step_id": "research_step",
      "step_name": "Research Information",
      "agent_id": "research_assistant",
      "function_name": "inference",
      "timeout_seconds": 60,
      "max_retries": 2,
      "parameters": {
        "prompt": "Research AI in healthcare",
        "max_tokens": 1024,
        "temperature": 0.3
      }
    }
  ]
}
```

#### List Workflows
```http
GET /api/v1/sequential-workflows
```

#### Get Workflow Details
```http
GET /api/v1/sequential-workflows/{workflow_id}
```

#### Execute Workflow (Synchronous)
```http
POST /api/v1/sequential-workflows/{workflow_id}/execute
Content-Type: application/json

{
  "input_context": {
    "additional_param": "value"
  }
}
```

#### Execute Workflow (Asynchronous)
```http
POST /api/v1/sequential-workflows/{workflow_id}/execute-async
Content-Type: application/json

{
  "input_context": {
    "additional_param": "value"
  }
}
```

#### Get Workflow Status
```http
GET /api/v1/sequential-workflows/{workflow_id}/status
```

#### Get Workflow Result
```http
GET /api/v1/sequential-workflows/{workflow_id}/result
```

#### Cancel Workflow
```http
POST /api/v1/sequential-workflows/{workflow_id}/cancel
```

#### Delete Workflow
```http
DELETE /api/v1/sequential-workflows/{workflow_id}
```

### Response Formats

#### Workflow Execution Result
```json
{
  "success": true,
  "data": {
    "workflow_id": "content_creation_workflow",
    "workflow_name": "Content Creation Pipeline",
    "success": true,
    "total_execution_time_ms": 15250.5,
    "total_steps": 3,
    "successful_steps": 3,
    "failed_steps": 0,
    "executed_steps": ["research_step", "writing_step", "review_step"],
    "step_results": {
      "research_step": {
        "success": true,
        "execution_time_ms": 5200.2,
        "result_data": {
          "research_summary": "AI in healthcare shows promising applications..."
        }
      }
    },
    "step_execution_times": {
      "research_step": 5200.2,
      "writing_step": 7800.1,
      "review_step": 2250.2
    }
  }
}
```

## Usage Examples

### Example 1: Content Creation Workflow

```python
import requests

# Create workflow
workflow = {
    "workflow_id": "content_creation_demo",
    "workflow_name": "Content Creation Demo",
    "description": "Demonstrate content creation workflow",
    "stop_on_failure": True,
    "global_context": {
        "topic": "Machine Learning Basics",
        "target_audience": "beginners"
    },
    "steps": [
        {
            "step_id": "research",
            "step_name": "Research Topic",
            "agent_id": "research_assistant",
            "function_name": "inference",
            "parameters": {
                "prompt": "Research machine learning basics for beginners",
                "max_tokens": 800,
                "temperature": 0.3
            }
        },
        {
            "step_id": "write",
            "step_name": "Write Content",
            "agent_id": "content_creator",
            "function_name": "inference",
            "parameters": {
                "prompt": "Write beginner-friendly content about machine learning",
                "max_tokens": 1200,
                "temperature": 0.7
            }
        }
    ]
}

# Register workflow
response = requests.post(
    'http://localhost:8080/api/v1/sequential-workflows',
    json=workflow
)

# Execute workflow
if response.status_code == 201:
    result = requests.post(
        f'http://localhost:8080/api/v1/sequential-workflows/{workflow["workflow_id"]}/execute'
    )
    print(result.json())
```

### Example 2: Data Analysis Workflow

```python
# Data analysis workflow configuration
data_workflow = {
    "workflow_id": "data_analysis_demo",
    "workflow_name": "Data Analysis Demo",
    "description": "Demonstrate data analysis workflow",
    "steps": [
        {
            "step_id": "prepare_data",
            "step_name": "Data Preparation",
            "agent_id": "data_analyst",
            "function_name": "data_analysis",
            "parameters": {
                "operation": "data_validation",
                "data_source": "sample_dataset"
            }
        },
        {
            "step_id": "analyze_data",
            "step_name": "Statistical Analysis",
            "agent_id": "data_analyst", 
            "function_name": "data_analysis",
            "parameters": {
                "operation": "statistical_summary"
            }
        },
        {
            "step_id": "generate_insights",
            "step_name": "Generate Insights",
            "agent_id": "research_assistant",
            "function_name": "inference",
            "parameters": {
                "prompt": "Generate insights from the data analysis",
                "max_tokens": 600,
                "temperature": 0.4
            }
        }
    ]
}
```

## Configuration

### Workflow Configuration Structure

```yaml
workflow_id: "unique_workflow_identifier"
workflow_name: "Human Readable Name"
description: "Detailed description of the workflow"
stop_on_failure: true  # Stop execution if any step fails
max_execution_time_seconds: 300  # Maximum total execution time
save_intermediate_results: true  # Save results from each step

global_context:
  # Global parameters available to all steps
  project_name: "Example Project"
  environment: "development"

steps:
  - step_id: "unique_step_id"
    step_name: "Human Readable Step Name"
    description: "Step description"
    agent_id: "target_agent_identifier"
    function_name: "function_to_execute"
    timeout_seconds: 60
    max_retries: 3
    continue_on_failure: false
    parameters:
      # Step-specific parameters
      param1: "value1"
      param2: "value2"
```

### Agent Configuration Requirements

Ensure your agents are properly configured in `config/agents.yaml`:

```yaml
agents:
  - name: "research_assistant"
    type: "research"
    capabilities: ["web_search", "text_processing", "data_analysis"]
    functions: ["inference", "web_search", "text_processing"]
    auto_start: true
    
  - name: "content_creator"
    type: "creative"
    capabilities: ["content_writing", "copywriting"]
    functions: ["inference", "text_processing"]
    auto_start: true
    
  - name: "data_analyst"
    type: "analytics"
    capabilities: ["data_analysis", "statistical_analysis"]
    functions: ["inference", "data_analysis"]
    auto_start: true
```

## Best Practices

### 1. **Workflow Design**
- **Single Responsibility**: Each step should have a single, well-defined purpose
- **Idempotency**: Design steps to be idempotent when possible
- **Error Boundaries**: Plan for failure scenarios and recovery strategies
- **Resource Management**: Consider timeout and retry settings carefully

### 2. **Context Management**
- **Minimize Context Size**: Keep context data concise to improve performance
- **Data Validation**: Validate data passed between steps
- **Type Consistency**: Maintain consistent data types across steps
- **Security**: Avoid passing sensitive data in context when possible

### 3. **Performance Optimization**
- **Parallel Processing**: Use parallel workflows for independent tasks
- **Caching**: Implement caching for expensive operations
- **Resource Pooling**: Manage agent resources efficiently
- **Monitoring**: Monitor execution times and optimize bottlenecks

### 4. **Error Handling**
- **Graceful Degradation**: Design workflows to handle partial failures
- **Retry Logic**: Implement appropriate retry strategies
- **Error Logging**: Log errors with sufficient detail for debugging
- **Fallback Mechanisms**: Provide fallback options for critical failures

## Monitoring and Debugging

### Execution Metrics

The system provides comprehensive metrics for monitoring:

- **Execution Time**: Total and per-step execution times
- **Success Rates**: Success/failure ratios for workflows and steps
- **Resource Usage**: Memory and CPU usage during execution
- **Queue Status**: Workflow queue depth and processing rates

### Debugging Tools

1. **Step-by-Step Logs**: Detailed logging for each step execution
2. **Context Inspection**: View context data at each step
3. **Error Traces**: Complete stack traces for failures
4. **Performance Profiling**: Identify performance bottlenecks

### Common Issues and Solutions

| Issue | Symptoms | Solution |
|-------|----------|----------|
| Step Timeout | Step fails with timeout error | Increase `timeout_seconds` or optimize agent function |
| Context Too Large | Slow performance, memory issues | Reduce context size, use references instead of full data |
| Agent Not Available | "Agent not found" errors | Ensure agent is configured and running |
| Function Not Found | "Function not found" errors | Verify function is registered with agent |
| Workflow Stuck | Workflow never completes | Check for deadlocks, increase timeout |

## Advanced Features

### 1. **Conditional Execution**
```python
# Step with precondition
step = {
    "step_id": "conditional_step",
    "precondition": lambda context: context.get("condition") == "true",
    # ... other step configuration
}
```

### 2. **Custom Result Processing**
```python
# Step with custom result processor
step = {
    "step_id": "processing_step",
    "result_processor": lambda context, result: process_custom_result(context, result),
    # ... other step configuration
}
```

### 3. **Workflow Templates**
Create reusable workflow templates that can be instantiated with different parameters.

### 4. **Dynamic Step Generation**
Generate workflow steps dynamically based on runtime conditions.

## Integration

### With Existing Agent System
The sequential workflow system integrates seamlessly with the existing agent system, using the same agents and functions.

### With External Systems
- **Webhooks**: Trigger workflows from external events
- **API Integration**: Call external APIs from workflow steps
- **Database Integration**: Store and retrieve workflow state from databases

## Troubleshooting

### Common Setup Issues

1. **Server Not Starting**: Check if port is available and configuration is valid
2. **Agents Not Loading**: Verify `config/agents.yaml` is correctly formatted
3. **Workflow Creation Fails**: Check JSON syntax and agent/function references
4. **Execution Timeout**: Increase timeout values or optimize agent functions

### Performance Issues

1. **Slow Execution**: Profile individual steps to identify bottlenecks
2. **Memory Usage**: Monitor context size and intermediate results
3. **Concurrent Execution**: Adjust maximum concurrent workflows

### Getting Help

- Check logs in `sequential_workflow_demo.log`
- Review agent configuration in `config/agents.yaml`
- Verify server status at `/v1/health`
- Check workflow status via API endpoints

## Future Enhancements

- **Visual Workflow Designer**: GUI for creating workflows
- **Conditional Branching**: Support for if/then/else logic in workflows
- **Loop Support**: Support for iterative steps
- **Workflow Versioning**: Version control for workflow definitions
- **Advanced Scheduling**: Cron-like scheduling for workflows
- **Workflow Composition**: Compose workflows from other workflows

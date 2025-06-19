# Kolosal Agents

This document provides an overview of the **agent** system in Kolosal Server, including the core concepts, architecture, and how to configure and use agents in your multi-agent system.

## What is an Agent?

An **Agent** in Kolosal is an autonomous, modular component capable of executing functions, handling messages, and collaborating with other agents. Agents can be configured with specific capabilities and functions, and communicate via a message routing system.

## Key Components

- **AgentCore**: The main class representing an agent. Handles lifecycle (start/stop), function execution, message handling, and capabilities.
- **FunctionManager**: Registers and manages functions that an agent can execute (e.g., add, echo, delay, text analysis, inference, etc.).
- **JobManager**: Handles asynchronous job execution for agent functions.
- **EventSystem**: Emits and handles agent-related events (e.g., message received).
- **MessageRouter**: Routes messages between agents.
- **ConfigurableAgentFactory**: Creates agent functions based on configuration (builtin, LLM, external API, etc.).
- **YAMLConfigurableAgentManager**: Loads agent and function configurations from YAML files, manages agent lifecycle, and supports hot-reloading.

## Agent Lifecycle

1. **Creation**: Agents are created from configuration or programmatically, with a unique ID, name, and type.
2. **Capabilities**: Agents can be assigned capabilities (e.g., text analysis, data transformation).
3. **Function Registration**: Functions are registered to agents, either built-in or custom.
4. **Message Routing**: Agents communicate by sending and receiving messages via the `MessageRouter`.
5. **Execution**: Agents can execute functions synchronously or asynchronously (jobs).
6. **Events**: Agents emit events for key actions (e.g., message received, function executed).

## Example Agent Configuration (YAML)

```yaml
agents:
  - name: "TextAgent"
    type: "text"
    capabilities: ["text_analysis", "echo"]
    functions: ["text_analysis", "echo"]
    auto_start: true
  - name: "InferenceAgent"
    type: "inference"
    capabilities: ["inference"]
    functions: ["inference"]
    auto_start: true
functions:
  - name: "text_analysis"
    type: "builtin"
    description: "Performs text analysis."
  - name: "inference"
    type: "inference"
    description: "Runs ML inference."
```

## Message Types

- `ping` / `pong`: Health check between agents.
- `greeting`: Send a greeting message.
- `function_request`: Request execution of a function.
- `function_response`: Response to a function request.

## Extending Agents

- **Add new functions** by implementing the `AgentFunction` interface and registering with the `FunctionManager`.
- **Configure new agents** via YAML or programmatically using the `YAMLConfigurableAgentManager`.
- **Integrate external APIs** or LLMs by defining new function types in the configuration.

## Demo & Status

The system supports a demonstration mode that prints agent status, capabilities, and registered functions. Use the `demonstrate_system()` method in `YAMLConfigurableAgentManager` for a summary.

---

For more details, see the main documentation and API references.

# Agent Demo - README

## Overview

The `agent_demo.py` script demonstrates the agent system capabilities of the Kolosal Server. It showcases how different types of agents can be used for various tasks using the downloaded LLM model.

## Features

- **Automatic Model Setup**: Automatically loads the Qwen 0.6B model from the downloads folder
- **Agent Discovery**: Discovers and lists all available agents from the server
- **Demo Scenarios**: Runs predefined scenarios to demonstrate different agent capabilities
- **Interactive Chat**: Allows real-time conversation with selected agents
- **Configuration Display**: Shows agent configuration details from the YAML file

## Prerequisites

1. **Kolosal Server Running**: Make sure the Kolosal Server is running on localhost:8080
2. **Model Downloaded**: Ensure `Qwen3-0.6B-UD-Q4_K_XL.gguf` is in the `downloads/` folder
3. **Agent Configuration**: The `config/agents.yaml` file should be properly configured

## Usage

### Basic Usage
```powershell
python agent_demo.py
```

### Custom Server Configuration
```powershell
python agent_demo.py --host localhost --port 8080 --config config/agents.yaml
```

## Available Agents

The demo works with the following agent types configured in `config/agents.yaml`:

1. **Research Assistant** (`research_assistant`)
   - Specializes in information research and analysis
   - Good for fact-finding and comprehensive answers

2. **Code Assistant** (`code_assistant`)
   - Helps with software development tasks
   - Code generation, review, and debugging

3. **Data Analyst** (`data_analyst`)
   - Performs data analysis and statistical work
   - Pattern recognition and insights extraction

4. **Content Creator** (`content_creator`)
   - Creates engaging written content
   - Articles, documentation, and creative writing

5. **Project Manager** (`project_manager`)
   - Coordinates tasks and manages projects
   - Planning and progress tracking

6. **QA Specialist** (`qa_specialist`)
   - Quality assurance and testing
   - Process improvement and validation

7. **Response Test Agent** (`response_test_agent`)
   - General-purpose testing agent
   - Good for basic interactions and testing

## Demo Options

### 1. View Available Agents
Lists all agents currently available on the server with their details.

### 2. Run Demo Scenarios
Executes predefined scenarios that demonstrate each agent's capabilities:
- Research Assistant: Technical research question
- Code Assistant: Programming task (factorial function)
- Data Analyst: Statistical explanation
- Content Creator: Blog post introduction
- Test Agent: General interaction

### 3. Interactive Chat
Select an agent and have a real-time conversation. Features:
- Type `quit`, `exit`, or `back` to return to main menu
- Type `clear` to clear conversation history
- All interactions are logged

### 4. Show Agent Configuration
Displays the configuration details for all agents loaded from the YAML file.

## Example Interaction

```
🚀 Kolosal Server Agent Demo
========================================
🔍 Checking server connection...
✅ Server is running

🔧 Setting up model engine...
✅ Found model file: downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf
✅ Engine 'qwen-0.6b-demo' created successfully

📋 Getting available agents...
✅ Found 7 agents

🎯 Demo Options:
1. View available agents
2. Run demo scenarios
3. Interactive chat with agent
4. Show agent configuration
5. Exit

Select option (1-5): 2

🎯 Running Demo Scenarios
============================================================

📋 Research Assistant - Technical Research
🤖 Agent: research_assistant
💬 Message: What are the key benefits of using large language models in software development?
📝 Response:
   Large language models offer several key benefits in software development...
```

## Troubleshooting

### Server Connection Issues
- Ensure Kolosal Server is running
- Check if the server is accessible at the specified host and port
- Verify server health endpoint is responding

### Model Loading Issues
- Confirm the model file exists in `downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf`
- Check server logs for model loading errors
- Ensure sufficient system resources for model loading

### Agent Not Found
- Verify `config/agents.yaml` is properly formatted
- Check server logs for agent loading errors
- Ensure agents are configured with correct model references

### API Endpoint Issues
The demo tries multiple endpoint patterns:
- `/v1/agents/{id}/execute`
- `/v1/agents/{name}/execute`  
- `/agents/{id}/execute`
- `/agents/{name}/execute`

If all fail, it falls back to direct chat completion API.

## Logging

All interactions are logged to:
- Console output (INFO level)
- `agent_demo.log` file (detailed logging)

Check the log file for detailed request/response information if troubleshooting is needed.

## Configuration

The demo uses the agent configuration from `config/agents.yaml`. Each agent has:
- **Name and Type**: Identifier and category
- **Role**: Description of the agent's purpose
- **System Prompt**: Instructions that define the agent's behavior
- **LLM Config**: Model settings (temperature, max tokens, etc.)
- **Functions**: Available capabilities

## Notes

- The demo automatically creates a model engine called `qwen-0.6b-demo`
- Conversation history is maintained during interactive sessions
- All agent interactions use the same underlying LLM model but with different system prompts
- Response quality depends on the model capabilities and system prompt design

# Kolosal CLI - Gemini-Like Command Line Interface

This document describes the new CLI functionality added to Kolosal Server, inspired by Google's Gemini CLI.

## Overview

The Kolosal CLI provides an interactive command-line interface similar to Gemini CLI, allowing users to:

- Execute AI inference directly from the command line
- Manage models, agents, and workflows
- Process files with AI assistance
- Run interactive chat sessions
- Access system information and status

## Quick Start

### Option 1: Using Launcher Scripts

**Windows:**
```bash
launch-cli.bat
```

**Linux/macOS:**
```bash
./launch-cli.sh
```

### Option 2: Manual Build and Run

1. Build the project:
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

2. Start CLI mode:
```bash
./kolosal-server --cli
```

## CLI Commands

### Basic Commands

| Command | Description | Usage |
|---------|-------------|-------|
| `help` | Show available commands | `help [command]` |
| `status` | Show server status | `status` |
| `exit` | Exit the CLI | `exit` |

### Model Management

| Command | Description | Usage |
|---------|-------------|-------|
| `models` | List/manage models | `models [list\|download\|remove] [model_name]` |
| `infer` | Run inference | `infer <prompt> [--model <model>] [--max-tokens <num>] [--temperature <temp>]` |

### Chat Interface

| Command | Description | Usage |
|---------|-------------|-------|
| `chat` | Quick chat | `chat [model_name] [message]` |
| `chat-interactive` | Interactive chat session | `chat-interactive [model_name]` |

### File Processing (Gemini @ equivalent)

| Command | Description | Usage |
|---------|-------------|-------|
| `process-file` | Analyze file content | `process-file <filepath> <prompt>` |

### Agent & Workflow Management

| Command | Description | Usage |
|---------|-------------|-------|
| `agents` | Manage agents | `agents [list\|create\|execute] [args...]` |
| `workflow` | Manage workflows | `workflow [list\|create\|execute\|status] [args...]` |
| `run-workflow` | Execute workflows | `run-workflow <type> [params...]` |

### System Information

| Command | Description | Usage |
|---------|-------------|-------|
| `config` | Show/modify config | `config [get\|set] [key] [value]` |
| `sysinfo` | System information | `sysinfo [--detailed]` |

## Special Commands (Gemini-style)

### Slash Commands (/)
- `/help` or `/?` - Show help
- `/clear` - Clear screen
- `/history` - Show command history
- `/version` - Show version info

### At Commands (@) - *Planned*
- `@<file>` - Include file content in prompt

### Shell Commands (!)
- `!<command>` - Execute shell command
- `!` - Toggle shell mode (planned)

## Example Usage

### Basic Inference
```bash
kolosal> infer "Explain quantum computing" --max-tokens 200 --temperature 0.7
```

### File Analysis
```bash
kolosal> process-file README.md "Summarize this documentation"
```

### Interactive Chat
```bash
kolosal> chat-interactive
You: What is machine learning?
AI: Machine learning is a subset of artificial intelligence...
You: exit
```

### Workflow Execution
```bash
kolosal> run-workflow content "AI in healthcare" "doctors"
kolosal> run-workflow code "REST API for user authentication"
kolosal> run-workflow analysis "customer satisfaction survey data"
```

### System Status
```bash
kolosal> status
kolosal> sysinfo --detailed
```

## Features

### 🎨 Rich Terminal Interface
- **Colored output** with ANSI codes
- **Beautiful welcome screen** with ASCII art
- **Syntax highlighting** for different command types
- **Progress indicators** for long-running operations

### 🤖 AI Integration
- **Direct inference calls** to loaded models
- **Interactive chat sessions** with conversation history
- **File processing** with context injection
- **Workflow automation** with predefined templates

### 🔧 Advanced Features
- **Command history** with navigation
- **Tab completion** (planned)
- **Configuration management** 
- **Real-time status monitoring**
- **Shell command execution**

### 🛡️ Safety Features
- **Confirmation prompts** for destructive operations
- **Input validation** and sanitization
- **Error handling** with helpful messages
- **Graceful shutdown** handling

## Architecture

The CLI is built with a modular architecture:

```
CLIInterface
├── Command Registry (pluggable commands)
├── Input Parser (arguments, quotes, escaping)
├── Special Command Handlers (/, @, !)
├── History Management
└── Advanced Commands
    ├── InferenceCommand
    ├── InteractiveChatCommand
    ├── FileProcessCommand
    ├── WorkflowExecuteCommand
    └── SystemInfoCommand
```

## Integration with Kolosal Server

The CLI integrates seamlessly with existing Kolosal Server components:

- **ServerAPI** - Access to server instance and configuration
- **NodeManager** - Model and engine management
- **AgentManager** - Agent system operations
- **AutoSetupManager** - Automated setup and configuration
- **Inference Engine** - Direct AI model access

## Comparison with Gemini CLI

| Feature | Gemini CLI | Kolosal CLI | Status |
|---------|------------|-------------|---------|
| Interactive REPL | ✅ | ✅ | Complete |
| Slash commands | ✅ | ✅ | Complete |
| @ file inclusion | ✅ | 🚧 | Planned |
| ! shell commands | ✅ | ✅ | Complete |
| Colored output | ✅ | ✅ | Complete |
| Model management | ✅ | ✅ | Complete |
| Chat sessions | ✅ | ✅ | Complete |
| MCP servers | ✅ | 🚧 | Via existing agent system |
| Tool confirmation | ✅ | 🚧 | Planned |
| Workflow automation | ➖ | ✅ | Kolosal exclusive |

## Development

### Adding New Commands

1. Create a new command class inheriting from `Command`:
```cpp
class MyCommand : public Command {
public:
    CommandResult execute(const std::vector<std::string>& args) override;
    std::string getName() const override { return "mycommand"; }
    std::string getDescription() const override { return "My custom command"; }
    std::string getUsage() const override { return "mycommand <arg>"; }
};
```

2. Register it in `CLIInterface::CLIInterface()`:
```cpp
registerCommand(std::make_unique<MyCommand>());
```

### Extending Special Commands

- **Slash commands**: Modify `handleSlashCommand()`
- **@ commands**: Modify `handleAtCommand()`  
- **! commands**: Modify `handleShellCommand()`

## Future Enhancements

- [ ] Tab completion for commands and file paths
- [ ] Real-time inference streaming
- [ ] Multi-file context (@dir/ support)
- [ ] Plugin system for custom commands
- [ ] Integration with external tools (git, npm, etc.)
- [ ] Scripting support for automation
- [ ] CLI themes and customization
- [ ] Remote server connection mode

## Contributing

To contribute to the CLI:

1. Follow the existing code style and patterns
2. Add tests for new commands
3. Update this documentation
4. Ensure cross-platform compatibility

## License

Same as Kolosal Server project license.

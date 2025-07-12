# Kolosal Server Demo Scripts

This directory contains demo scripts that showcase the working functionality of Kolosal Server.

## Available Demos

### 1. 🚀 `working_demo.py` - Main Working Demo
**Status: ✅ WORKS**

This is the primary demo that showcases all the functionality that works right now:
- OpenAI-compatible chat completions
- OpenAI-compatible text completions  
- Server health monitoring
- Model and engine status
- Creative tasks (code generation, problem solving, etc.)

```bash
python working_demo.py
```

### 2. 🤖 `simple_agent_demo.py` - Agent System Demo
**Status: ⚠️ PARTIAL (agents load but inference engines not configured)**

This demo tests the agent system specifically:
- Lists available agents
- Tests agent capabilities
- Shows what's working vs what needs configuration

```bash
python simple_agent_demo.py
```

### 3. 📚 `retrieval_agent_demo.py` - RAG Demo
**Status: ❌ REQUIRES QDRANT (original demo)**

This is the original retrieval demo that requires Qdrant vector database:
- Document indexing and retrieval
- RAG (Retrieval-Augmented Generation)
- Agent-based document processing

```bash
python retrieval_agent_demo.py
```

### 4. 🔗 `openai_client_demo.py` - OpenAI Client Integration
**Status: ✅ WORKS (requires `pip install openai`)**

This demo shows how to use Kolosal Server with the official OpenAI Python client:
- Drop-in replacement for OpenAI API
- Standard OpenAI client library usage
- Multi-turn conversations

```bash
pip install openai
python openai_client_demo.py
```

## What's Working ✅

- **OpenAI-compatible API endpoints** (`/v1/chat/completions`, `/v1/completions`)
- **Model management** (qwen3-0.6b model loaded and working)
- **Health monitoring** (`/health` endpoint)
- **Basic server functionality**
- **Agent system** (agents load and run, but inference needs configuration)

## What Needs Setup ⚠️

- **Agent inference engines** (agents can't access the model)
- **Qdrant vector database** (for document retrieval features)
- **Embedding models** (for semantic search)

## Quick Test

To quickly verify the server is working:

```bash
curl http://localhost:8080/health
curl http://localhost:8080/models
```

Or use the working demo:

```bash
python working_demo.py
```

## Integration Examples

The server works with standard OpenAI client libraries:

```python
from openai import OpenAI

client = OpenAI(
    api_key="dummy-key",
    base_url="http://localhost:8080/v1"
)

response = client.chat.completions.create(
    model="qwen3-0.6b",
    messages=[{"role": "user", "content": "Hello!"}]
)
```

## Next Steps

1. **Use working functionality**: Start with `working_demo.py` to see what works
2. **Fix agent inference engines**: Configure agents to use available models  
3. **Set up Qdrant**: For document retrieval and RAG features
4. **Test embeddings**: Once vector database is configured

The core API functionality is solid and OpenAI-compatible! 🎉

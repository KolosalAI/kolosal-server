# Git Merge Conflict Resolution Guide

## Overview
You have merge conflicts between the `dev` branch and the `rag-agent-retrieval` branch. Here's a systematic approach to resolve them.

## Resolution Strategy

### 1. Configuration Files (Keep new structure)
- **config.example.yaml** & **config.yaml**: These were deleted in rag-agent-retrieval. Keep them deleted since the new structure uses `config/` directory.

### 2. Key Principle for Resolution
- **Agent System**: Take the more comprehensive implementation from rag-agent-retrieval
- **Model Management**: Merge both LLM and embedding support
- **Routes**: Take the newer route implementations
- **Build System**: Use Windows/MSVC settings for your environment

## Step-by-Step Resolution

### Phase 1: Remove deleted files
```bash
git rm config.example.yaml config.yaml
git rm src/routes/chat_completion_route.cpp
git rm src/routes/inference_chat_completion_route.cpp
git rm src/routes/inference_completion_route.cpp
```

### Phase 2: Agent System Files (Take rag-agent-retrieval version)
For these files, take the rag-agent-retrieval version which has more comprehensive implementations:
- include/kolosal/agents/agent_data.hpp
- include/kolosal/agents/builtin_functions.hpp
- include/kolosal/agents/function_manager.hpp
- include/kolosal/agents/multi_agent_system.hpp
- include/kolosal/agents/yaml_config.hpp

### Phase 3: Model Management (Merge both approaches)
For these files, we need to carefully merge to support both LLM and embedding models:
- include/kolosal/download_manager.hpp
- include/kolosal/node_manager.h
- src/download_manager.cpp
- src/node_manager.cpp

### Phase 4: Routes and API (Take newer implementations)
- include/kolosal/routes/models_route.hpp
- src/routes/models_route.cpp
- src/server_api.cpp

### Phase 5: Build and Infrastructure
- CMakeLists.txt
- inference/src/inference.cpp
- src/server_config.cpp

## Automated Commands

Run these commands in order:

```bash
# Phase 1: Remove deleted files
git rm config.example.yaml config.yaml
git rm src/routes/chat_completion_route.cpp
git rm src/routes/inference_chat_completion_route.cpp
git rm src/routes/inference_completion_route.cpp

# Phase 2: Accept rag-agent-retrieval version for agent files
git checkout --theirs include/kolosal/agents/agent_data.hpp
git checkout --theirs include/kolosal/agents/builtin_functions.hpp
git checkout --theirs include/kolosal/agents/function_manager.hpp
git checkout --theirs include/kolosal/agents/multi_agent_system.hpp
git checkout --theirs include/kolosal/agents/yaml_config.hpp

# Phase 3: Accept rag-agent-retrieval for routes
git checkout --theirs include/kolosal/routes/models_route.hpp
git checkout --theirs src/routes/models_route.cpp
git checkout --theirs src/server_api.cpp

# Phase 4: Accept rag-agent-retrieval for config
git checkout --theirs src/server_config.cpp

# Add resolved files
git add .
```

After running these commands, you'll still need to manually resolve:
1. CMakeLists.txt
2. include/kolosal/download_manager.hpp
3. include/kolosal/node_manager.h
4. inference/src/inference.cpp
5. src/download_manager.cpp
6. src/node_manager.cpp

## Manual Resolution Guidelines

### For Model Management Files:
- Keep embedding model support from HEAD
- Keep retrieval and agent functionality from rag-agent-retrieval
- Merge both approaches for comprehensive model handling

### For Build Files:
- Use Windows/MSVC settings for your development environment
- Keep newer inference engine improvements

## Final Steps
```bash
# After resolving all conflicts manually
git add .
git commit -m "Resolve merge conflicts between dev and rag-agent-retrieval"
```

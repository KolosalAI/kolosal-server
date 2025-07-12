#!/usr/bin/env python3
"""
Simple Agent Demo for Kolosal Server
This script demonstrates basic agent functionality without requiring document retrieval.
"""

import requests
import json
import time
import sys
from typing import Dict, List, Any

# Server configuration
SERVER_URL = "http://localhost:8080"
HEADERS = {"Content-Type": "application/json"}

def check_server_health():
    """Check if the server is running and healthy."""
    try:
        response = requests.get(f"{SERVER_URL}/health", timeout=5)
        return response.status_code == 200
    except requests.RequestException:
        return False

def list_agents() -> List[Dict[str, Any]]:
    """Get list of available agents."""
    try:
        response = requests.get(f"{SERVER_URL}/api/v1/agents", headers=HEADERS)
        if response.status_code == 200:
            data = response.json()
            return data.get("data", [])
        else:
            print(f"❌ Failed to list agents: {response.status_code}")
            return []
    except requests.RequestException as e:
        print(f"❌ Error listing agents: {e}")
        return []

def get_agent_uuid_by_name(agent_name: str) -> str:
    """Get agent UUID by name."""
    agents = list_agents()
    for agent in agents:
        if agent.get("name") == agent_name:
            return agent.get("uuid", agent.get("id", ""))
    return ""

def test_simple_text_generation(agent_name: str):
    """Test basic text generation without requiring external dependencies."""
    print(f"\n🤖 Testing {agent_name} agent's text generation...")
    
    agent_uuid = get_agent_uuid_by_name(agent_name)
    if not agent_uuid:
        print(f"   ❌ Agent '{agent_name}' not found")
        return False
    
    # Simple prompts that don't require external knowledge
    test_prompts = [
        "Hello! Please introduce yourself.",
        "What is 2 + 2?",
        "Count from 1 to 5.",
        "Write a short greeting message."
    ]
    
    for prompt in test_prompts:
        print(f"\n💭 Prompt: {prompt}")
        
        try:
            response = requests.post(
                f"{SERVER_URL}/api/v1/agents/{agent_uuid}/execute",
                headers=HEADERS,
                json={
                    "function": "inference",
                    "parameters": {
                        "prompt": prompt,
                        "max_tokens": 100,
                        "temperature": 0.7
                    }
                }
            )
            
            if response.status_code == 200:
                result = response.json()
                if result.get("data", {}).get("success", False):
                    answer = result.get("data", {}).get("result", {}).get("text", "")
                    print(f"   ✅ Response: {answer}")
                    return True
                else:
                    error = result.get("data", {}).get("error", "Unknown error")
                    print(f"   ❌ Execution failed: {error}")
            else:
                print(f"   ❌ Request failed: {response.status_code}")
                try:
                    error_data = response.json()
                    print(f"   Error details: {error_data}")
                except:
                    print(f"   Error response: {response.text}")
                    
        except requests.RequestException as e:
            print(f"   ❌ Error: {e}")
        
        time.sleep(0.5)  # Small delay between requests
    
    return False

def test_agent_capabilities(agent_name: str):
    """Test what capabilities an agent actually has."""
    print(f"\n🔍 Testing {agent_name} agent capabilities...")
    
    agent_uuid = get_agent_uuid_by_name(agent_name)
    if not agent_uuid:
        print(f"   ❌ Agent '{agent_name}' not found")
        return
    
    # Get agent details
    agents = list_agents()
    agent = None
    for a in agents:
        if a.get("name") == agent_name:
            agent = a
            break
    
    if agent:
        print(f"   📋 Agent ID: {agent.get('uuid', 'unknown')}")
        print(f"   📋 Agent Type: {agent.get('type', 'unknown')}")
        print(f"   📋 Running: {agent.get('running', False)}")
        print(f"   📋 Capabilities: {', '.join(agent.get('capabilities', []))}")
    
    # Test simple math function if available
    print(f"\n🧮 Testing basic functionality...")
    
    try:
        # Try a simple text processing task
        response = requests.post(
            f"{SERVER_URL}/api/v1/agents/{agent_uuid}/execute",
            headers=HEADERS,
            json={
                "function": "text_processing",
                "parameters": {
                    "text": "Hello World",
                    "operation": "uppercase"
                }
            }
        )
        
        if response.status_code == 200:
            result = response.json()
            print(f"   ✅ Text processing available")
            print(f"   Result: {result.get('data', {}).get('result', 'No result')}")
        else:
            print(f"   ⚠️ Text processing not available")
            
    except Exception as e:
        print(f"   ⚠️ Text processing test failed: {e}")

def test_chat_completion():
    """Test the basic OpenAI-compatible chat completion endpoint."""
    print(f"\n💬 Testing OpenAI-compatible chat completion...")
    
    try:
        response = requests.post(
            f"{SERVER_URL}/v1/chat/completions",
            headers=HEADERS,
            json={
                "model": "qwen3-0.6b",
                "messages": [
                    {"role": "user", "content": "Hello! How are you today?"}
                ],
                "max_tokens": 50,
                "temperature": 0.7
            }
        )
        
        if response.status_code == 200:
            result = response.json()
            message = result.get("choices", [{}])[0].get("message", {}).get("content", "")
            print(f"   ✅ Chat completion successful!")
            print(f"   Response: {message}")
            return True
        else:
            print(f"   ❌ Chat completion failed: {response.status_code}")
            print(f"   Response: {response.text}")
            
    except requests.RequestException as e:
        print(f"   ❌ Error: {e}")
    
    return False

def test_basic_completion():
    """Test the basic OpenAI-compatible text completion endpoint."""
    print(f"\n📝 Testing OpenAI-compatible text completion...")
    
    try:
        response = requests.post(
            f"{SERVER_URL}/v1/completions",
            headers=HEADERS,
            json={
                "model": "qwen3-0.6b",
                "prompt": "The weather today is",
                "max_tokens": 30,
                "temperature": 0.7
            }
        )
        
        if response.status_code == 200:
            result = response.json()
            text = result.get("choices", [{}])[0].get("text", "")
            print(f"   ✅ Text completion successful!")
            print(f"   Response: {text}")
            return True
        else:
            print(f"   ❌ Text completion failed: {response.status_code}")
            print(f"   Response: {response.text}")
            
    except requests.RequestException as e:
        print(f"   ❌ Error: {e}")
    
    return False

def check_models():
    """Check what models are available."""
    print(f"\n🔧 Checking available models...")
    
    try:
        response = requests.get(f"{SERVER_URL}/models", headers=HEADERS)
        if response.status_code == 200:
            result = response.json()
            models = result.get("data", [])
            print(f"   📋 Found {len(models)} models:")
            for model in models:
                print(f"      - {model.get('id', 'unknown')}")
            return models
        else:
            print(f"   ❌ Failed to get models: {response.status_code}")
            
    except requests.RequestException as e:
        print(f"   ❌ Error: {e}")
    
    return []

def check_engines():
    """Check what engines are available."""
    print(f"\n⚙️ Checking available engines...")
    
    try:
        response = requests.get(f"{SERVER_URL}/engines", headers=HEADERS)
        if response.status_code == 200:
            result = response.json()
            engines = result.get("engines", [])
            print(f"   📋 Found {len(engines)} engines:")
            for engine in engines:
                print(f"      - {engine.get('engine_id', 'unknown')} (Status: {engine.get('status', 'unknown')})")
            return engines
        else:
            print(f"   ❌ Failed to get engines: {response.status_code}")
            
    except requests.RequestException as e:
        print(f"   ❌ Error: {e}")
    
    return []

def main():
    """Main demo function."""
    print("🚀 Simple Kolosal Server Agent Demo")
    print("=" * 50)
    
    # Check server health
    print("🏥 Checking server health...")
    if not check_server_health():
        print("❌ Server is not running or not healthy!")
        print("   Please start the Kolosal server first.")
        sys.exit(1)
    print("✅ Server is running and healthy")
    
    # Check available models and engines
    models = check_models()
    engines = check_engines()
    
    # Test basic OpenAI-compatible endpoints first
    chat_works = test_chat_completion()
    completion_works = test_basic_completion()
    
    if not chat_works and not completion_works:
        print("\n⚠️ Basic inference endpoints are not working. This might be due to:")
        print("   - Model not loaded")
        print("   - Engine configuration issues")
        print("   - Inference service not initialized")
    
    # List available agents
    print("\n🤖 Listing available agents...")
    agents = list_agents()
    
    if not agents:
        print("❌ No agents found!")
        return
    
    print(f"   📋 Found {len(agents)} agents:")
    for agent in agents:
        agent_name = agent.get('name', agent.get('id', 'unknown'))
        status = "🟢 Running" if agent.get('running', False) else "🔴 Stopped"
        capabilities = agent.get('capabilities', [])
        print(f"      - {agent_name} ({status}) - {len(capabilities)} capabilities")
    
    # Test a few agents
    test_agents = ['response_test_agent', 'code_assistant', 'data_analyst']
    
    for agent_name in test_agents:
        if any(a.get('name') == agent_name for a in agents):
            print(f"\n" + "="*60)
            test_agent_capabilities(agent_name)
            
            # Try the text generation test
            if not test_simple_text_generation(agent_name):
                print(f"   ⚠️ {agent_name} text generation not working")
        else:
            print(f"\n⚠️ Agent '{agent_name}' not found")
    
    print("\n✅ Demo completed!")
    print("\n📝 Summary:")
    print("   - Server health check completed")
    print("   - Model and engine status checked")
    print("   - Basic inference endpoints tested")
    print("   - Agent capabilities explored")
    print("\n💡 Tips:")
    print("   - If agents aren't responding, check engine status")
    print("   - Make sure the qwen3-0.6b model is loaded")
    print("   - For document retrieval, Qdrant vector database is required")

if __name__ == "__main__":
    main()

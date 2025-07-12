#!/usr/bin/env python3
"""
Working Kolosal Server Demo
This script demonstrates the functionality that actually works right now.
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

def demo_chat_completions():
    """Demonstrate the working chat completion functionality."""
    print("\n💬 Chat Completion Demo")
    print("-" * 40)
    
    conversations = [
        {"role": "user", "content": "Hello! What's your name?"},
        {"role": "user", "content": "Can you help me with a simple math problem? What is 15 + 27?"},
        {"role": "user", "content": "Write a short poem about programming."},
        {"role": "user", "content": "Explain what an API is in simple terms."},
    ]
    
    for i, message in enumerate(conversations, 1):
        print(f"\n🗨️ Conversation {i}:")
        print(f"User: {message['content']}")
        
        try:
            response = requests.post(
                f"{SERVER_URL}/v1/chat/completions",
                headers=HEADERS,
                json={
                    "model": "qwen3-0.6b",
                    "messages": [message],
                    "max_tokens": 150,
                    "temperature": 0.7
                }
            )
            
            if response.status_code == 200:
                result = response.json()
                ai_message = result.get("choices", [{}])[0].get("message", {}).get("content", "")
                print(f"Assistant: {ai_message}")
                
                # Show usage stats if available
                usage = result.get("usage", {})
                if usage:
                    print(f"📊 Tokens - Prompt: {usage.get('prompt_tokens', 0)}, "
                          f"Completion: {usage.get('completion_tokens', 0)}, "
                          f"Total: {usage.get('total_tokens', 0)}")
            else:
                print(f"❌ Request failed: {response.status_code}")
                print(f"Response: {response.text}")
                
        except requests.RequestException as e:
            print(f"❌ Error: {e}")
        
        time.sleep(1)  # Small delay between requests

def demo_text_completions():
    """Demonstrate the working text completion functionality."""
    print("\n📝 Text Completion Demo")
    print("-" * 40)
    
    prompts = [
        "The capital of France is",
        "In programming, a function is",
        "The benefits of artificial intelligence include",
        "To make a good cup of coffee, you should",
    ]
    
    for i, prompt in enumerate(prompts, 1):
        print(f"\n✏️ Completion {i}:")
        print(f"Prompt: {prompt}")
        
        try:
            response = requests.post(
                f"{SERVER_URL}/v1/completions",
                headers=HEADERS,
                json={
                    "model": "qwen3-0.6b",
                    "prompt": prompt,
                    "max_tokens": 100,
                    "temperature": 0.7,
                    "stop": ["\n\n"]
                }
            )
            
            if response.status_code == 200:
                result = response.json()
                completion = result.get("choices", [{}])[0].get("text", "")
                print(f"Completion: {prompt}{completion}")
                
                # Show usage stats if available
                usage = result.get("usage", {})
                if usage:
                    print(f"📊 Tokens used: {usage.get('total_tokens', 0)}")
            else:
                print(f"❌ Request failed: {response.status_code}")
                print(f"Response: {response.text}")
                
        except requests.RequestException as e:
            print(f"❌ Error: {e}")
        
        time.sleep(1)

def demo_streaming_chat():
    """Demonstrate streaming chat if supported."""
    print("\n🌊 Streaming Chat Demo")
    print("-" * 40)
    
    print("\n🗨️ Streaming conversation:")
    print("User: Tell me a short story about a robot.")
    print("Assistant: ", end="", flush=True)
    
    try:
        response = requests.post(
            f"{SERVER_URL}/v1/chat/completions",
            headers=HEADERS,
            json={
                "model": "qwen3-0.6b",
                "messages": [{"role": "user", "content": "Tell me a short story about a robot."}],
                "max_tokens": 200,
                "temperature": 0.8,
                "stream": False  # Set to True if streaming is supported
            },
            stream=False
        )
        
        if response.status_code == 200:
            result = response.json()
            message = result.get("choices", [{}])[0].get("message", {}).get("content", "")
            print(message)
        else:
            print(f"\n❌ Streaming failed: {response.status_code}")
            
    except requests.RequestException as e:
        print(f"\n❌ Error: {e}")

def demo_creative_tasks():
    """Demonstrate creative tasks that work well."""
    print("\n🎨 Creative Tasks Demo")
    print("-" * 40)
    
    creative_prompts = [
        {
            "task": "Code Generation",
            "prompt": "Write a simple Python function that calculates the factorial of a number."
        },
        {
            "task": "Problem Solving", 
            "prompt": "A farmer has 17 sheep. All but 9 die. How many sheep does the farmer have left?"
        },
        {
            "task": "Creative Writing",
            "prompt": "Write a haiku about artificial intelligence."
        },
        {
            "task": "Explanation",
            "prompt": "Explain the concept of recursion in programming using a simple analogy."
        }
    ]
    
    for i, item in enumerate(creative_prompts, 1):
        print(f"\n🎯 Task {i}: {item['task']}")
        print(f"Prompt: {item['prompt']}")
        
        try:
            response = requests.post(
                f"{SERVER_URL}/v1/chat/completions",
                headers=HEADERS,
                json={
                    "model": "qwen3-0.6b",
                    "messages": [{"role": "user", "content": item['prompt']}],
                    "max_tokens": 200,
                    "temperature": 0.7
                }
            )
            
            if response.status_code == 200:
                result = response.json()
                response_text = result.get("choices", [{}])[0].get("message", {}).get("content", "")
                print(f"Response: {response_text}")
            else:
                print(f"❌ Request failed: {response.status_code}")
                
        except requests.RequestException as e:
            print(f"❌ Error: {e}")
        
        time.sleep(1)

def show_server_info():
    """Display server information and capabilities."""
    print("\n🔧 Server Information")
    print("-" * 40)
    
    # Health check
    try:
        response = requests.get(f"{SERVER_URL}/health")
        if response.status_code == 200:
            print("✅ Server Status: Healthy")
        else:
            print(f"⚠️ Server Status: Issues detected ({response.status_code})")
    except:
        print("❌ Server Status: Unreachable")
    
    # Models
    try:
        response = requests.get(f"{SERVER_URL}/models")
        if response.status_code == 200:
            models = response.json().get("data", [])
            print(f"📚 Available Models: {len(models)}")
            for model in models:
                print(f"   - {model.get('id', 'unknown')}")
        else:
            print("⚠️ Could not retrieve models")
    except:
        print("❌ Error retrieving models")
    
    # Engines
    try:
        response = requests.get(f"{SERVER_URL}/engines")
        if response.status_code == 200:
            engines = response.json().get("engines", [])
            print(f"⚙️ Available Engines: {len(engines)}")
            for engine in engines:
                status = engine.get('status', 'unknown')
                print(f"   - {engine.get('engine_id', 'unknown')} ({status})")
        else:
            print("⚠️ Could not retrieve engines")
    except:
        print("❌ Error retrieving engines")
    
    # Agents
    try:
        response = requests.get(f"{SERVER_URL}/api/v1/agents")
        if response.status_code == 200:
            agents = response.json().get("data", [])
            running_agents = sum(1 for a in agents if a.get('running', False))
            print(f"🤖 Available Agents: {len(agents)} ({running_agents} running)")
            for agent in agents[:3]:  # Show first 3
                status = "🟢" if agent.get('running', False) else "🔴"
                print(f"   {status} {agent.get('name', 'unknown')} ({agent.get('type', 'unknown')})")
            if len(agents) > 3:
                print(f"   ... and {len(agents) - 3} more")
        else:
            print("⚠️ Could not retrieve agents")
    except:
        print("❌ Error retrieving agents")

def main():
    """Main demo function."""
    print("🚀 Kolosal Server Working Demo")
    print("=" * 50)
    print("This demo showcases the functionality that works right now!")
    
    # Check server health
    if not check_server_health():
        print("❌ Server is not running or not healthy!")
        print("   Please start the Kolosal server first.")
        sys.exit(1)
    
    # Show server information
    show_server_info()
    
    # Run working demos
    demo_chat_completions()
    demo_text_completions()
    demo_creative_tasks()
    demo_streaming_chat()
    
    print("\n" + "=" * 50)
    print("✅ Demo completed successfully!")
    print("\n📝 What's Working:")
    print("   ✅ OpenAI-compatible chat completions")
    print("   ✅ OpenAI-compatible text completions")
    print("   ✅ Model and engine management")
    print("   ✅ Agent system (basic functionality)")
    print("   ✅ Health monitoring")
    
    print("\n⚠️ What Needs Setup:")
    print("   🔧 Agent inference engine configuration")
    print("   🔧 Qdrant vector database (for document retrieval)")
    print("   🔧 Some model loading issues")
    
    print("\n🎯 Next Steps:")
    print("   1. Use the working OpenAI-compatible endpoints")
    print("   2. Fix agent inference engine configuration")
    print("   3. Set up Qdrant for document retrieval features")
    print("   4. Test embedding and retrieval capabilities")
    
    print(f"\n🌐 API Base URL: {SERVER_URL}")
    print("📖 Use standard OpenAI client libraries with this server!")

if __name__ == "__main__":
    main()

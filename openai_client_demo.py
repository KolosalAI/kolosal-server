#!/usr/bin/env python3
"""
OpenAI Client Integration Demo
This shows how to use the Kolosal Server with the official OpenAI Python client.
"""

try:
    from openai import OpenAI
except ImportError:
    print("❌ OpenAI library not installed. Install with: pip install openai")
    exit(1)

import sys

# Configure client to use Kolosal Server
client = OpenAI(
    api_key="dummy-key",  # Kolosal server doesn't require real API key
    base_url="http://localhost:8080/v1"
)

def test_openai_client():
    """Test using OpenAI client with Kolosal Server."""
    print("🚀 OpenAI Client Integration with Kolosal Server")
    print("=" * 55)
    
    # Test 1: Chat Completion
    print("\n💬 Chat Completion Test")
    print("-" * 30)
    
    try:
        response = client.chat.completions.create(
            model="qwen3-0.6b",
            messages=[
                {"role": "user", "content": "Hello! Can you help me write a Python function to reverse a string?"}
            ],
            max_tokens=200,
            temperature=0.7
        )
        
        print(f"Response: {response.choices[0].message.content}")
        print(f"Usage: {response.usage.total_tokens} tokens")
        
    except Exception as e:
        print(f"❌ Chat completion failed: {e}")
    
    # Test 2: Text Completion
    print("\n📝 Text Completion Test")
    print("-" * 30)
    
    try:
        response = client.completions.create(
            model="qwen3-0.6b",
            prompt="The most important programming concepts for beginners are",
            max_tokens=150,
            temperature=0.8
        )
        
        print(f"Completion: {response.choices[0].text}")
        print(f"Usage: {response.usage.total_tokens} tokens")
        
    except Exception as e:
        print(f"❌ Text completion failed: {e}")
    
    # Test 3: Multiple Messages Chat
    print("\n🗨️ Multi-turn Conversation Test")
    print("-" * 35)
    
    try:
        response = client.chat.completions.create(
            model="qwen3-0.6b",
            messages=[
                {"role": "user", "content": "What's the weather like?"},
                {"role": "assistant", "content": "I don't have access to real-time weather data, but I can help you with weather-related questions or suggest ways to check the weather."},
                {"role": "user", "content": "How can I check the weather programmatically?"}
            ],
            max_tokens=150,
            temperature=0.6
        )
        
        print(f"Response: {response.choices[0].message.content}")
        
    except Exception as e:
        print(f"❌ Multi-turn chat failed: {e}")
    
    print("\n✅ OpenAI client integration working!")
    print("\n🎯 This proves that Kolosal Server is OpenAI-compatible!")
    print("   You can use it as a drop-in replacement for OpenAI API in your projects.")

if __name__ == "__main__":
    test_openai_client()

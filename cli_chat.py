#!/usr/bin/env python3
"""
Simple CLI Chat Client for Kolosal Server

A lightweight command-line interface to chat with the model loaded in Kolosal Server.
Make sure the server is running and a model engine is loaded before using this client.

Usage:
    python cli_chat.py [--host HOST] [--port PORT] [--model MODEL]
"""

import requests
import json
import argparse
import sys
from typing import List, Dict, Any

class KolosalChatClient:
    def __init__(self, host: str = "localhost", port: int = 8080, model: str = "qwen-0.6b"):
        self.base_url = f"http://{host}:{port}"
        self.model = model
        self.session = requests.Session()
        self.conversation_history: List[Dict[str, str]] = []
        
    def add_engine_if_needed(self):
        """Add the model engine if it doesn't exist"""
        try:
            # Check if engine exists
            response = self.session.get(f"{self.base_url}/engines")
            if response.status_code == 200:
                engines = response.json()
                if isinstance(engines, list):
                    existing_models = [e.get('engine_id') for e in engines if isinstance(e, dict)]
                    if self.model in existing_models:
                        print(f"✅ Model '{self.model}' is already loaded")
                        return True
            
            # Add the engine
            print(f"🔄 Loading model '{self.model}'...")
            engine_data = {
                "engine_id": self.model,
                "model_path": "downloads/Qwen3-0.6B-UD-Q4_K_XL.gguf",
                "n_ctx": 2048,
                "n_gpu_layers": 0,
                "main_gpu_id": 0
            }
            
            response = self.session.post(f"{self.base_url}/engines", json=engine_data)
            if response.status_code in [200, 201]:
                print(f"✅ Model '{self.model}' loaded successfully")
                return True
            else:
                print(f"❌ Failed to load model: {response.text}")
                return False
                
        except requests.exceptions.RequestException as e:
            print(f"❌ Error connecting to server: {e}")
            return False
    
    def send_message(self, message: str) -> str:
        """Send a message to the model and get response"""
        try:
            # Add user message to conversation
            self.conversation_history.append({"role": "user", "content": message})
            
            # Prepare request data
            request_data = {
                "model": self.model,
                "messages": self.conversation_history.copy(),
                "stream": False,
                "temperature": 0.7,
                "max_tokens": 500
            }
            
            # Send request
            response = self.session.post(
                f"{self.base_url}/v1/chat/completions",
                json=request_data,
                headers={"Content-Type": "application/json"}
            )
            
            if response.status_code == 200:
                result = response.json()
                if "choices" in result and len(result["choices"]) > 0:
                    assistant_message = result["choices"][0]["message"]["content"]
                    # Add assistant response to conversation
                    self.conversation_history.append({"role": "assistant", "content": assistant_message})
                    return assistant_message
                else:
                    return "❌ No response from model"
            else:
                return f"❌ Error: {response.status_code} - {response.text}"
                
        except requests.exceptions.RequestException as e:
            return f"❌ Connection error: {e}"
        except json.JSONDecodeError as e:
            return f"❌ JSON error: {e}"
        except Exception as e:
            return f"❌ Unexpected error: {e}"
    
    def check_server_health(self) -> bool:
        """Check if the server is running and healthy"""
        try:
            response = self.session.get(f"{self.base_url}/v1/health", timeout=5)
            return response.status_code == 200
        except:
            return False
    
    def start_chat(self):
        """Start the interactive chat session"""
        print("🤖 Kolosal Server CLI Chat")
        print("=" * 40)
        print(f"Server: {self.base_url}")
        print(f"Model: {self.model}")
        print()
        
        # Check server health
        print("🔍 Checking server connection...")
        if not self.check_server_health():
            print("❌ Cannot connect to Kolosal Server")
            print("Make sure the server is running at", self.base_url)
            return
        
        print("✅ Server is running")
        
        # Load model
        if not self.add_engine_if_needed():
            print("❌ Failed to load model. Exiting.")
            return
        
        print()
        print("💬 Chat started! Type 'quit', 'exit', or 'bye' to end the session.")
        print("📝 Type 'clear' to clear conversation history.")
        print("=" * 40)
        print()
        
        while True:
            try:
                # Get user input
                user_input = input("You: ").strip()
                
                if not user_input:
                    continue
                
                # Check for exit commands
                if user_input.lower() in ['quit', 'exit', 'bye']:
                    print("\n👋 Goodbye!")
                    break
                
                # Check for clear command
                if user_input.lower() == 'clear':
                    self.conversation_history.clear()
                    print("🧹 Conversation history cleared!")
                    continue
                
                # Send message and get response
                print("🤖 Assistant: ", end="", flush=True)
                response = self.send_message(user_input)
                print(response)
                print()
                
            except KeyboardInterrupt:
                print("\n\n👋 Chat interrupted. Goodbye!")
                break
            except EOFError:
                print("\n\n👋 Goodbye!")
                break

def main():
    parser = argparse.ArgumentParser(description="CLI Chat Client for Kolosal Server")
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--model", default="qwen-0.6b", help="Model name (default: qwen-0.6b)")
    
    args = parser.parse_args()
    
    client = KolosalChatClient(args.host, args.port, args.model)
    client.start_chat()

if __name__ == "__main__":
    main()

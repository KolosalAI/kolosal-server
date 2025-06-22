#!/usr/bin/env python3
"""
Enhanced example of testing agent response generation in Kolosal Server with model download

This demonstrates how to:
1. Download a small model to the server
2. Create an agent with the downloaded model configuration
3. Send messages to the agent
4. Test the agent's response generation capabilities
5. Clean up the downloaded model

Usage:
    python example_agent_response_test.py
"""

import requests
import json
import time
import uuid

def download_test_model(session, base_url):
    """Download a small test model to the server"""
    print("📥 Downloading test model...")
    
    # Check if model already exists first
    engine_id = "test-qwen-0.6b"
    if check_model_availability(session, base_url, engine_id):
        print(f"✅ Model '{engine_id}' already exists and is available!")
        return True, engine_id
    
    # If model doesn't exist or isn't available, try to create it
    # Small model configuration for testing
    model_config = {
        "engine_id": engine_id,
        "model_path": "https://huggingface.co/kolosal/qwen3-0.6b/resolve/main/Qwen3-0.6B-UD-Q4_K_XL.gguf",
        "load_immediately": True,  # Load immediately for testing
        "main_gpu_id": 0,
        "loading_parameters": {
            "n_ctx": 2048,
            "n_gpu_layers": 0,  # Use CPU for compatibility
            "n_batch": 512,
            "n_ubatch": 512,
            "use_mmap": True,
            "use_mlock": False,
            "cont_batching": True,
            "warmup": True,
            "n_parallel": 1,
            "n_keep": 0
        }
    }
    
    try:
        print(f"  Requesting model download: {model_config['model_path']}")
        response = session.post(
            f"{base_url}/engines",
            json=model_config,
            timeout=300  # 5 minutes timeout for download
        )
        
        print(f"  Status Code: {response.status_code}")
        
        if response.status_code in [200, 201]:
            print("✅ Model download initiated successfully!")
            try:
                json_response = response.json()
                print(f"  Response: {json.dumps(json_response, indent=2)}")
                
                # Wait for model to be ready
                if wait_for_model_ready(session, base_url, engine_id):
                    return True, engine_id
                else:
                    return False, None
                
            except json.JSONDecodeError:
                print(f"  Response (text): {response.text}")
                return True, engine_id  # Assume success if we can't parse JSON        else:
            print(f"❌ Model download failed. Status: {response.status_code}")
            try:
                error_response = response.json()
                print(f"  Error: {json.dumps(error_response, indent=2)}")
                
                # Check if it's because the model already exists
                if response.status_code == 409 and 'engine_id_exists' in str(error_response):
                    print("ℹ️  Model already exists, using existing model...")
                    if check_model_availability(session, base_url, engine_id):
                        return True, engine_id
                    
            except json.JSONDecodeError:
                print(f"  Error (text): {response.text}")
            return False, None
            
    except requests.exceptions.Timeout:
        print("⏰ Model download timed out (this is normal for large models)")
        print("  Checking if model is available...")
        if check_model_availability(session, base_url, engine_id):
            return True, engine_id
        else:
            return False, None
    except requests.exceptions.RequestException as e:
        print(f"❌ Network error during model download: {e}")
        return False, None

def wait_for_model_ready(session, base_url, engine_id, max_wait=300):
    """Wait for model to be ready (loaded and available)"""
    print(f"⏳ Waiting for model '{engine_id}' to be ready...")
    
    start_time = time.time()
    while time.time() - start_time < max_wait:
        try:
            # Check model status
            response = session.get(f"{base_url}/engines/{engine_id}/status")
            if response.status_code == 200:
                model_info = response.json()
                if model_info.get('status') == 'loaded' or model_info.get('loaded', False):
                    print("✅ Model is ready!")
                    return True
                else:
                    print(f"  Model status: {model_info.get('status', 'unknown')}")
            
            time.sleep(10)  # Wait 10 seconds before checking again
            
        except requests.exceptions.RequestException:
            time.sleep(5)
    
    print("⚠️  Timeout waiting for model to be ready, proceeding anyway...")
    return True

def check_model_availability(session, base_url, engine_id):
    """Check if model is available/loaded"""
    try:
        response = session.get(f"{base_url}/engines/{engine_id}/status")
        if response.status_code == 200:
            model_info = response.json()
            print(f"✅ Model found: {json.dumps(model_info, indent=2)}")
            return True
        else:
            print(f"⚠️  Model not found or not ready: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"❌ Error checking model availability: {e}")
        return False

def list_available_models(session, base_url):
    """List all available models on the server"""
    print("📋 Listing available models...")
    
    try:
        response = session.get(f"{base_url}/engines")
        if response.status_code == 200:
            models = response.json()
            print(f"Available models: {json.dumps(models, indent=2)}")
            return models
        else:
            print(f"⚠️  Failed to list models: {response.status_code}")
            return []
    except requests.exceptions.RequestException as e:
        print(f"❌ Error listing models: {e}")
        return []

def test_agent_response_generation():
    """Enhanced example of testing agent response generation with model download"""
    
    # Server configuration
    BASE_URL = "http://localhost:8080"
      # Create a session for making requests
    session = requests.Session()
    session.headers.update({
        'Content-Type': 'application/json',
        'User-Agent': 'AgentResponseTester/1.0'
    })
    
    model_downloaded = False
    agent_created = False
    engine_id = None  # Initialize engine_id
    
    try:
        # 1. First, list available models
        available_models = list_available_models(session, BASE_URL)
        
        # 2. Clean up any existing test resources before starting
        print("\n🧹 Cleaning up any existing test resources...")
        try:
            # Try to delete existing test agent
            delete_response = session.delete(f"{BASE_URL}/api/v1/agents/response-test-agent")
            if delete_response.status_code in [200, 204]:
                print("✅ Cleaned up existing test agent")
            elif delete_response.status_code == 404:
                print("ℹ️  No existing test agent to clean up")
        except Exception as e:
            print(f"ℹ️  Note: {e}")
        
        # Also try to clean up existing test model if needed
        try:
            # Check if test model exists and remove it to ensure clean test
            if check_model_availability(session, BASE_URL, "test-qwen-0.6b"):
                print("ℹ️  Found existing test model, keeping it for reuse...")
            else:
                print("ℹ️  No existing test model found")
        except Exception as e:
            print(f"ℹ️  Note: {e}")
        
        # 3. Download a test model (or use existing one)
        model_downloaded, engine_id = download_test_model(session, BASE_URL)
        
        if not model_downloaded:
            print("❌ Failed to download model, cannot proceed with agent testing")
            return
        
        # 4. Create an agent with the downloaded model configuration
        print("\n🤖 Creating test agent with downloaded model...")
        agent_config = {
            "name": "response-test-agent",
            "type": "general",
            "role": "General purpose test agent for response validation",
            "system_prompt": """You are a helpful AI assistant designed for testing purposes. You provide clear,
accurate, and concise responses to various types of questions and requests.
You can handle greetings, math problems, coding tasks, and explanations.""",
            "capabilities": [
                "text_processing",
                "general_assistance",
                "math_calculation", 
                "code_generation",
                "explanation"
            ],
            "functions": [
                "text_processing"
            ],            "llm_config": {
                "model_name": engine_id,  # Use the downloaded model
                "api_endpoint": "",  # Let the server handle the endpoint
                "instruction": "You are a helpful AI assistant. Provide clear, accurate, and concise responses.",
                "temperature": 0.5,
                "max_tokens": 1024,
                "timeout_seconds": 30,
                "max_retries": 3
            },
            "auto_start": True,
            "max_concurrent_jobs": 5,
            "heartbeat_interval_seconds": 10,
            "custom_settings": {
                "response_format": "conversational",
                "enable_testing": True
            }
        }        # Create the agent using the correct API endpoint
        response = session.post(f"{BASE_URL}/api/v1/agents", json=agent_config)
        if response.status_code in [200, 201]:
            print("✅ Agent created successfully with local model!")
            try:
                response_json = response.json()
                print(f"Response: {json.dumps(response_json, indent=2)}")
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
            agent_created = True
            
            # Wait a moment for agent to initialize
            print("⏳ Waiting for agent to initialize...")
            time.sleep(5)
        else:
            print(f"❌ Failed to create agent. Status: {response.status_code}")
            try:
                error_json = response.json()
                print(f"Error: {json.dumps(error_json, indent=2)}")
            except json.JSONDecodeError:
                print(f"Error (text): {response.text}")
            return# Verify the agent was created by listing agents
        print("\n📋 Verifying agent creation...")
        agent_exists = False
        try:
            agents_response = session.get(f"{BASE_URL}/api/v1/agents")
            if agents_response.status_code == 200:
                agents = agents_response.json()
                print(f"Available agents: {json.dumps(agents, indent=2)}")
                
                # Check if our agent is in the list
                agent_found = False
                if isinstance(agents, dict) and 'data' in agents:
                    for agent in agents['data']:
                        if agent.get('name') == 'response-test-agent':
                            agent_found = True
                            print(f"✅ Found agent in data array: {agent}")
                            break
                elif isinstance(agents, list):
                    for agent in agents:
                        if agent.get('name') == 'response-test-agent':
                            agent_found = True
                            print(f"✅ Found agent in list: {agent}")
                            break
                elif isinstance(agents, dict) and 'response-test-agent' in agents:
                    agent_found = True
                    print(f"✅ Found agent in dict: {agents['response-test-agent']}")
                
                if agent_found:
                    print("✅ Agent verified in the agents list")
                    agent_exists = True
                else:
                    print("⚠️  Agent not found in the agents list")
            else:
                print(f"⚠️  Failed to list agents: {agents_response.status_code}")
                try:
                    error_data = agents_response.json()
                    print(f"Error details: {json.dumps(error_data, indent=2)}")
                except:
                    print(f"Error text: {agents_response.text}")
        except Exception as e:
            print(f"⚠️  Error verifying agent: {e}")
          # Check agent status specifically
        print(f"\n🔍 Checking agent status directly...")
        try:
            status_response = session.get(f"{BASE_URL}/api/v1/agents/response-test-agent")
            print(f"Agent direct check status: {status_response.status_code}")
            if status_response.status_code == 200:
                agent_info = status_response.json()
                print(f"Agent info: {json.dumps(agent_info, indent=2)}")
                agent_exists = True
            else:
                print(f"Agent not accessible directly: {status_response.status_code}")
                try:
                    error_data = status_response.json()
                    print(f"Error: {json.dumps(error_data, indent=2)}")
                except:
                    print(f"Error text: {status_response.text}")
        except Exception as e:
            print(f"Error checking agent directly: {e}")
        
        # Also check system status
        print(f"\n🔍 Checking agent system status...")
        try:
            system_response = session.get(f"{BASE_URL}/api/v1/agents/system/status")
            print(f"System status check: {system_response.status_code}")
            if system_response.status_code == 200:
                system_info = system_response.json()
                print(f"System info: {json.dumps(system_info, indent=2)}")
            else:
                print(f"System status not accessible: {system_response.status_code}")
        except Exception as e:
            print(f"Error checking system status: {e}")
        
        # Only proceed with testing if agent exists
        if not agent_exists:
            print("❌ Agent verification failed - cannot proceed with testing")
            return
          # 4. Test different types of messages with the local model
        test_messages = [
            {
                "function": "text_processing",
                "parameters": {
                    "text": "Hello! Can you introduce yourself?",
                    "operation": "analyze"
                },
                "description": "Basic greeting and introduction"
            },
            {
                "function": "text_processing", 
                "parameters": {
                    "text": "What is 15 + 27?",
                    "operation": "analyze"
                },
                "description": "Simple math question"
            },
            {
                "function": "text_processing",
                "parameters": {
                    "text": "Write a short Python function to reverse a string.",
                    "operation": "analyze"  
                },
                "description": "Technical assistance request"
            },
            {
                "function": "text_processing",
                "parameters": {
                    "text": "Explain what Python is in one sentence.",
                    "operation": "analyze"
                },                "description": "Simple explanation request"
            }
        ]
        
        print(f"\n💬 Testing agent responses with local model...")
        print("=" * 60)
        
        # First, check what endpoints are available for our agent
        print(f"\n🔍 Checking available endpoints for agent 'response-test-agent'...")
        test_endpoints = [
            f"/api/v1/agents/response-test-agent",
            f"/api/v1/agents/response-test-agent/status",
            f"/api/v1/agents/response-test-agent/info"
        ]
        
        for endpoint in test_endpoints:
            try:
                response = session.get(f"{BASE_URL}{endpoint}")
                print(f"  GET {endpoint}: {response.status_code}")
                if response.status_code == 200:
                    try:
                        data = response.json()
                        print(f"    Response: {json.dumps(data, indent=4)}")
                    except:
                        print(f"    Response (text): {response.text}")
            except Exception as e:
                print(f"  GET {endpoint}: Error - {e}")
        
        # Use the correct API endpoint for agent execution
        chat_endpoint = f"/api/v1/agents/response-test-agent/execute"
        successful_responses = 0
        
        for i, test_case in enumerate(test_messages, 1):
            print(f"\n📝 Test {i}: {test_case['description']}")
            print(f"Function: {test_case['function']}")
            print(f"Text: {test_case['parameters']['text']}")
            print("-" * 50)
            
            # Use the request data directly from test_case
            request_data = {
                "function": test_case["function"],
                "parameters": test_case["parameters"]
            }
            
            # Try the correct agent execute endpoint
            response_received = False
            
            try:
                print(f"  Trying endpoint: {chat_endpoint}")
                
                # Execute the agent function
                response = session.post(f"{BASE_URL}{chat_endpoint}", json=request_data, timeout=90)
                
                print(f"  Status Code: {response.status_code}")
                
                if response.status_code == 200:
                    try:
                        json_response = response.json()
                        print(f"  ✅ Success! Agent Response:")
                        
                        # Extract the actual response content based on agent API structure
                        if 'data' in json_response:
                            data = json_response['data']
                            if 'result' in data:
                                # Handle agent result data
                                result_data = data['result']
                                if isinstance(result_data, dict):
                                    # Look for text, message, or other response fields
                                    if 'text' in result_data:
                                        print(f"     {result_data['text']}")
                                    elif 'message' in result_data:
                                        print(f"     {result_data['message']}")
                                    elif 'response' in result_data:
                                        print(f"     {result_data['response']}")
                                    else:
                                        print(f"     {json.dumps(result_data, indent=2)}")
                                else:
                                    print(f"     {result_data}")
                            elif 'text' in data:
                                print(f"     {data['text']}")
                            elif 'message' in data:
                                print(f"     {data['message']}")
                            else:
                                print(f"     {json.dumps(data, indent=2)}")
                        elif 'success' in json_response and json_response['success']:
                            # Look for response content in different possible fields
                            if 'result' in json_response:
                                print(f"     {json.dumps(json_response['result'], indent=2)}")
                            elif 'text' in json_response:
                                print(f"     {json_response['text']}")
                            elif 'message' in json_response:
                                print(f"     {json_response['message']}")
                            else:
                                print(f"     {json.dumps(json_response, indent=2)}")
                        else:
                            print(f"     {json.dumps(json_response, indent=2)}")
                        
                        successful_responses += 1
                        response_received = True
                        
                    except json.JSONDecodeError:
                        if response.text.strip():
                            print(f"  ✅ Success! Agent Response (text):")
                            print(f"     {response.text}")
                            successful_responses += 1
                            response_received = True
                else:
                    print(f"  ⚠️  Error: {response.status_code}")
                    try:
                        error_json = response.json()
                        print(f"     {json.dumps(error_json, indent=2)}")
                    except:
                        print(f"     {response.text}")
                        
            except requests.exceptions.Timeout:
                print(f"  ⏰ Timeout for endpoint: {chat_endpoint}")
            except requests.exceptions.RequestException as e:
                print(f"  ❌ Request failed for {chat_endpoint}: {e}")
            
            if not response_received:
                print("  ❌ No successful response from agent")
              # Small delay between requests
            time.sleep(2)
        
        print(f"\n📊 Test Results Summary:")
        print(f"✅ Successful responses: {successful_responses}/{len(test_messages)}")
        print(f"📈 Success rate: {(successful_responses/len(test_messages)*100):.1f}%")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        
    finally:
        # 5. Clean up - delete the test agent and model
        print(f"\n🧹 Cleaning up...")
        
        if agent_created:
            try:
                delete_response = session.delete(f"{BASE_URL}/api/v1/agents/response-test-agent")
                if delete_response.status_code in [200, 204, 404]:
                    print("✅ Test agent cleaned up successfully")
                else:
                    print(f"⚠️  Failed to delete test agent: {delete_response.status_code}")
            except Exception as e:
                print(f"⚠️  Error cleaning up agent: {e}")
        
        if model_downloaded and engine_id:
            try:
                # Try to unload/remove the model
                unload_response = session.delete(f"{BASE_URL}/engines/{engine_id}")
                if unload_response.status_code in [200, 204, 404]:
                    print("✅ Test model cleaned up successfully")
                else:
                    print(f"⚠️  Failed to clean up test model: {unload_response.status_code}")
            except Exception as e:
                print(f"⚠️  Error cleaning up model: {e}")

def main():
    print("🚀 Enhanced Agent Response Generation Test with Model Download")
    print("=" * 70)
    print("This test will:")
    print("1. List currently available models")
    print("2. Download a small test model (Qwen 0.6B)")
    print("3. Create a test agent using the downloaded model")
    print("4. Send various messages to test response generation")
    print("5. Clean up the test agent and model")
    print("=" * 70)
    
    # Test server connectivity first
    try:
        print("\n🔗 Testing server connectivity...")
        response = requests.get("http://localhost:8080/health", timeout=5)
        print(f"✅ Server is reachable (Status: {response.status_code})")
    except requests.exceptions.RequestException as e:
        print(f"❌ Cannot reach server at http://localhost:8080: {e}")
        print("Please make sure the Kolosal Server is running.")
        return
    
    # Run the enhanced test
    test_agent_response_generation()
    
    print("\n🏁 Enhanced test completed!")
    print("=" * 70)

if __name__ == "__main__":
    main()

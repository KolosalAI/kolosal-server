#!/usr/bin/env python3
"""
Enhanced example of testing agent response generation in Kolosal Server using existing agents

This demonstrates how to:
1. Get available agents from the server
2. Select an appropriate agent for testing
3. Send messages to the agent using the /v1/agents/{id}/execute endpoint
4. Test the agent's response generation capabilities
5. Display test results and statistics

Usage:
    python example_agent_response_test_updated.py
"""

import requests
import json
import time
import uuid
import logging
import os

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('agent_response_test.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

def log_request_response(action, url, request_data, response):
    """Helper function to log request-response pairs"""
    try:
        # Log request
        logger.info(f"{action} Request - URL: {url}")
        if request_data:
            logger.info(f"{action} Request - Data: {json.dumps(request_data, indent=2)}")
        
        # Log response
        logger.info(f"{action} Response - Status: {response.status_code}")
        logger.info(f"{action} Response - Headers: {dict(response.headers)}")
        
        try:
            json_response = response.json()
            logger.info(f"{action} Response - JSON: {json.dumps(json_response, indent=2)}")
        except json.JSONDecodeError:
            logger.info(f"{action} Response - Text: {response.text}")
    except Exception as e:
        logger.error(f"Error logging {action} request-response: {e}")

def download_test_model(session, base_url):
    """Download a small test model to the server or use existing one"""
    print("📥 Checking for test model...")
    
    engine_id = "test-qwen-0.6b"
    
    # First, check if we have the model file locally
    has_local_file, local_path = check_local_model_file()
    if has_local_file:
        print(f"✅ Found local model file: {local_path}")
        
        # Check if engine already exists
        if check_model_availability(session, base_url, engine_id):
            print(f"✅ Engine '{engine_id}' already exists and is available!")
            return True, engine_id
        
        # Try to create engine with local file
        if create_engine_with_local_model(session, base_url, engine_id, local_path):
            # Wait for it to be ready
            if wait_for_model_ready(session, base_url, engine_id, max_wait=60):
                return True, engine_id
            else:
                print("⚠️  Local model engine created but readiness uncertain, proceeding anyway...")
                return True, engine_id
    
    # Check if model exists in the engines list
    existing_models = list_available_models(session, base_url)
    if existing_models:
        for model in existing_models:
            if isinstance(model, dict) and model.get('engine_id') == engine_id:
                print(f"✅ Model '{engine_id}' found in engines list!")
                # Try to load it if it's not already loaded
                if not model.get('loaded', False):
                    print(f"  Model exists but not loaded, attempting to load...")
                    try:
                        load_url = f"{base_url}/engines/{engine_id}/load"
                        load_response = session.post(load_url)
                        log_request_response("Model Load", load_url, None, load_response)
                        
                        if load_response.status_code in [200, 201]:
                            print("✅ Model loaded successfully!")
                            # Wait for it to be ready
                            if wait_for_model_ready(session, base_url, engine_id, max_wait=60):
                                return True, engine_id
                        else:
                            print(f"⚠️  Failed to load model: {load_response.status_code}")
                    except Exception as e:
                        print(f"⚠️  Error loading model: {e}")
                
                return True, engine_id
    
    # Check if model is available and ready
    if check_model_availability(session, base_url, engine_id):
        print(f"✅ Model '{engine_id}' already exists and is available!")
        return True, engine_id
    
    # If model doesn't exist, try to create it by downloading
    print(f"  Model '{engine_id}' not found, attempting to download...")
    model_config = {
        "engine_id": engine_id,
        "model_path": "https://huggingface.co/kolosal/qwen3-0.6b/resolve/main/Qwen3-0.6B-UD-Q4_K_XL.gguf",
        "load_immediately": True,
        "main_gpu_id": 0,
        "loading_parameters": {
            "n_ctx": 2048,
            "n_gpu_layers": 0,
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
        url = f"{base_url}/engines"
        
        response = session.post(url, json=model_config, timeout=300)
        log_request_response("Model Download", url, model_config, response)
        
        print(f"  Status Code: {response.status_code}")
        
        if response.status_code in [200, 201]:
            print("✅ Model download initiated successfully!")
            try:
                json_response = response.json()
                print(f"  Response: {json.dumps(json_response, indent=2)}")
            except json.JSONDecodeError:
                print(f"  Response (text): {response.text}")
            
            # Wait for model to be ready
            if wait_for_model_ready(session, base_url, engine_id):
                return True, engine_id
            else:
                # Even if wait fails, try to use the model
                print("⚠️  Wait timeout, but attempting to use model anyway...")
                return True, engine_id
                
        elif response.status_code == 409:
            print("ℹ️  Model already exists (409 Conflict)")
            try:
                error_response = response.json()
                print(f"  Server response: {json.dumps(error_response, indent=2)}")
            except json.JSONDecodeError:
                print(f"  Server response (text): {response.text}")
            
            # Model exists, try to use it
            print("  Attempting to use existing model...")
            if check_model_availability(session, base_url, engine_id):
                return True, engine_id
            else:
                # Try to load the existing model
                try:
                    load_url = f"{base_url}/engines/{engine_id}/load"
                    load_response = session.post(load_url)
                    log_request_response("Model Load After Conflict", load_url, None, load_response)
                    
                    if load_response.status_code in [200, 201]:
                        print("✅ Existing model loaded successfully!")
                        if wait_for_model_ready(session, base_url, engine_id, max_wait=60):
                            return True, engine_id
                    
                    # Even if load fails, try to use the model
                    print("⚠️  Load failed, but attempting to use model anyway...")
                    return True, engine_id
                except Exception as e:
                    print(f"⚠️  Error loading existing model: {e}")
                    return True, engine_id  # Still try to use it
        else:
            print(f"❌ Model download failed. Status: {response.status_code}")
            try:
                error_response = response.json()
                print(f"  Error: {json.dumps(error_response, indent=2)}")
            except json.JSONDecodeError:
                print(f"  Error (text): {response.text}")
            
            # Last resort: check if model somehow exists anyway
            print("  Checking if model exists despite error...")
            if check_model_availability(session, base_url, engine_id):
                print("  Model found despite error, proceeding...")
                return True, engine_id
            
            return False, None
            
    except requests.exceptions.Timeout:
        print("⏰ Model download timed out (this is normal for large models)")
        print("  Checking if model is available...")
        if check_model_availability(session, base_url, engine_id):
            return True, engine_id
        else:
            print("  Model not ready after timeout, but proceeding anyway...")
            return True, engine_id  # Try to use it anyway
    except requests.exceptions.RequestException as e:
        print(f"❌ Network error during model download: {e}")
        # Check if model exists despite network error
        print("  Checking if model exists despite network error...")
        if check_model_availability(session, base_url, engine_id):
            return True, engine_id
        return False, None

def wait_for_model_ready(session, base_url, engine_id, max_wait=300):
    """Wait for model to be ready (loaded and available)"""
    print(f"⏳ Waiting for model '{engine_id}' to be ready...")
    
    start_time = time.time()
    attempts = 0
    max_attempts = max_wait // 10  # Check every 10 seconds
    
    while time.time() - start_time < max_wait and attempts < max_attempts:
        attempts += 1
        try:
            # Check model status
            url = f"{base_url}/engines/{engine_id}/status"
            response = session.get(url)
            
            log_request_response(f"Model Status Check {attempts}", url, None, response)
            
            if response.status_code == 200:
                try:
                    model_info = response.json()
                    status = model_info.get('status', 'unknown')
                    print(f"  Attempt {attempts}: Model status: {status}")
                    
                    # Check various possible ready states
                    if (status == 'loaded' or 
                        model_info.get('loaded', False) or
                        model_info.get('ready', False) or
                        model_info.get('available', False)):
                        print("✅ Model is ready!")
                        return True
                    elif status in ['loading', 'downloading']:
                        print(f"  Model is {status}, continuing to wait...")
                    else:
                        print(f"  Model status: {status}")
                        
                except json.JSONDecodeError:
                    print(f"  Attempt {attempts}: Response (text): {response.text}")
                    # If we get a response, assume it's working
                    if response.text.strip():
                        print("  Got response, assuming model is working...")
                        return True
            elif response.status_code == 404:
                print(f"  Attempt {attempts}: Model not found (404)")
                # Model might not be created yet, continue waiting
            else:
                print(f"  Attempt {attempts}: Status code {response.status_code}")
            
            if attempts < max_attempts:
                print(f"  Waiting 10 seconds before next check... ({attempts}/{max_attempts})")
                time.sleep(10)
            
        except requests.exceptions.RequestException as e:
            print(f"  Attempt {attempts}: Network error: {e}")
            time.sleep(5)
    
    # Final check - if we've waited long enough, try one more time
    print(f"⚠️  Wait timeout after {attempts} attempts, doing final check...")
    try:
        url = f"{base_url}/engines/{engine_id}/status"
        response = session.get(url)
        if response.status_code == 200:
            print("✅ Model responded on final check, considering it ready!")
            return True
    except:
        pass
    
    print("⚠️  Model readiness uncertain, but proceeding anyway...")
    return True  # Return True to allow the test to continue

def check_model_availability(session, base_url, engine_id):
    """Check if model is available/loaded"""
    try:
        # First try the status endpoint
        url = f"{base_url}/engines/{engine_id}/status"
        response = session.get(url)
        
        log_request_response("Model Availability Check", url, None, response)
        
        if response.status_code == 200:
            try:
                model_info = response.json()
                print(f"✅ Model status: {json.dumps(model_info, indent=2)}")
                
                # Check various possible status indicators
                if (model_info.get('status') == 'loaded' or 
                    model_info.get('loaded', False) or
                    model_info.get('ready', False) or
                    model_info.get('available', False)):
                    return True
                else:
                    print(f"  Model exists but status is: {model_info.get('status', 'unknown')}")
                    # Even if not loaded, return True so we can try to use it
                    return True
                    
            except json.JSONDecodeError:
                print(f"  Model status response (text): {response.text}")
                # If we get a response but can't parse JSON, assume it exists
                return True
        elif response.status_code == 404:
            print(f"  Model '{engine_id}' not found (404)")
            return False
        else:
            print(f"  Model status check returned: {response.status_code}")
            # For other status codes, try the engines list as fallback
            
    except requests.exceptions.RequestException as e:
        print(f"  Error checking model status: {e}")
    
    # Fallback: check if model is in the engines list
    try:
        print("  Fallback: Checking engines list...")
        engines_url = f"{base_url}/engines"
        engines_response = session.get(engines_url)
        
        log_request_response("Engines List Fallback", engines_url, None, engines_response)
        
        if engines_response.status_code == 200:
            try:
                engines = engines_response.json()
                for engine in engines:
                    if isinstance(engine, dict) and engine.get('engine_id') == engine_id:
                        print(f"  Model '{engine_id}' found in engines list!")
                        return True
                        
                print(f"  Model '{engine_id}' not found in engines list")
                return False
                
            except json.JSONDecodeError:
                print(f"  Engines list response (text): {engines_response.text}")
                return False
        else:
            print(f"  Failed to get engines list: {engines_response.status_code}")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"  Error checking engines list: {e}")
        return False

def list_available_models(session, base_url):
    """List all available models on the server"""
    print("📋 Listing available models...")
    
    try:
        url = f"{base_url}/engines"
        response = session.get(url)
        
        # Log the request-response
        log_request_response("List Models", url, None, response)
        
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

def check_local_model_file(model_name="Qwen3-0.6B-UD-Q4_K_XL.gguf"):
    """Check if model file exists locally in downloads directory"""
    # Check common locations where the model might be stored
    possible_paths = [
        os.path.join("downloads", model_name),
        os.path.join(".", "downloads", model_name),
        os.path.join(os.getcwd(), "downloads", model_name),
        model_name  # In case it's in current directory
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            print(f"✅ Found local model file: {path}")
            return True, path
    
    print(f"ℹ️  Model file '{model_name}' not found locally")
    return False, None

def create_engine_with_local_model(session, base_url, engine_id, local_model_path):
    """Create engine using local model file"""
    print(f"🔧 Creating engine with local model: {local_model_path}")
    
    model_config = {
        "engine_id": engine_id,
        "model_path": local_model_path,
        "load_immediately": True,
        "main_gpu_id": 0,
        "loading_parameters": {
            "n_ctx": 2048,
            "n_gpu_layers": 0,
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
        url = f"{base_url}/engines"
        response = session.post(url, json=model_config, timeout=60)
        log_request_response("Local Model Engine Creation", url, model_config, response)
        
        if response.status_code in [200, 201]:
            print("✅ Engine created with local model!")
            return True
        elif response.status_code == 409:
            print("ℹ️  Engine already exists with this model")
            return True
        else:
            print(f"⚠️  Failed to create engine: {response.status_code}")
            try:
                error_response = response.json()
                print(f"  Error: {json.dumps(error_response, indent=2)}")
            except:
                print(f"  Error (text): {response.text}")
            return False
            
    except Exception as e:
        print(f"❌ Error creating engine with local model: {e}")
        return False

def get_agent_by_id(session, base_url, agent_id):
    """Get specific agent details by ID/UUID"""
    print(f"🔍 Getting agent details for ID: {agent_id}")
    
    try:
        url = f"{base_url}/api/v1/agents/{agent_id}"
        response = session.get(url)
        
        log_request_response("Get Agent By ID", url, None, response)
        
        if response.status_code == 200:
            try:
                agent_data = response.json()
                print(f"✅ Agent details: {json.dumps(agent_data, indent=2)}")
                return agent_data
            except json.JSONDecodeError:
                print(f"✅ Agent details (text): {response.text}")
                return {"status": "found", "raw_response": response.text}
        else:
            print(f"⚠️  Failed to get agent details: {response.status_code}")
            try:
                error_response = response.json()
                print(f"Error: {json.dumps(error_response, indent=2)}")
            except:
                print(f"Error (text): {response.text}")
            return None
            
    except requests.exceptions.RequestException as e:
        print(f"❌ Error getting agent details: {e}")
        return None

def get_available_agents(session, base_url):
    """Get list of available agents from the server"""
    print("🤖 Getting available agents...")
    
    try:
        url = f"{base_url}/api/v1/agents"
        response = session.get(url)
        
        log_request_response("Get Agents", url, None, response)
        
        if response.status_code == 200:
            try:
                agents_data = response.json()
                print(f"✅ Found agents: {json.dumps(agents_data, indent=2)}")
                
                # Extract agent information from the response
                agents = []
                agents_list = []
                
                if isinstance(agents_data, list):
                    agents_list = agents_data
                elif isinstance(agents_data, dict):
                    if 'agents' in agents_data:
                        agents_list = agents_data['agents']
                    elif 'data' in agents_data:
                        agents_list = agents_data['data']
                    else:
                        # If the response is a single dict, treat it as one agent
                        agents_list = [agents_data]
                
                for agent in agents_list:
                    if isinstance(agent, dict):
                        agent_info = {
                            'uuid': agent.get('uuid', agent.get('id', '')),
                            'id': agent.get('id', ''),  
                            'name': agent.get('name', 'Unknown Agent')
                        }
                        
                        # Prefer uuid over id for the identifier
                        agent_identifier = agent_info['uuid'] or agent_info['id']
                        
                        if agent_identifier:
                            agents.append({
                                'identifier': agent_identifier,
                                'uuid': agent_info['uuid'],
                                'id': agent_info['id'],
                                'name': agent_info['name']
                            })
                
                if agents:
                    print(f"📋 Available agents:")
                    for agent in agents:
                        print(f"  - Name: {agent['name']}")
                        print(f"    UUID: {agent['uuid']}")
                        print(f"    ID: {agent['id']}")
                        print(f"    Identifier: {agent['identifier']}")
                        print()
                
                return agents
                
            except json.JSONDecodeError:
                print(f"⚠️  Could not parse agents response: {response.text}")
                return []
        else:
            print(f"⚠️  Failed to get agents: {response.status_code}")
            try:
                error_response = response.json()
                print(f"Error: {json.dumps(error_response, indent=2)}")
            except:
                print(f"Error (text): {response.text}")
            return []
            
    except requests.exceptions.RequestException as e:
        print(f"❌ Error getting agents: {e}")
        return []

def get_agent_system_status(session, base_url):
    """Get agent system status"""
    print("📊 Checking agent system status...")
    
    try:
        url = f"{base_url}/api/v1/agents/system/status"
        response = session.get(url)
        
        log_request_response("Agent System Status", url, None, response)
        
        if response.status_code == 200:
            try:
                status_data = response.json()
                print(f"✅ Agent system status: {json.dumps(status_data, indent=2)}")
                return status_data
            except json.JSONDecodeError:
                print(f"✅ Agent system status (text): {response.text}")
                return {"status": "running"}
        else:
            print(f"⚠️  Failed to get agent system status: {response.status_code}")
            return None
            
    except requests.exceptions.RequestException as e:
        print(f"❌ Error getting agent system status: {e}")
        return None

def test_agent_response_generation():
    """Test agent response generation using existing agents on the server"""
    
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
    engine_id = None
    selected_agent_id = None
    
    try:
        # 0. Initial diagnostics
        print("\n🔍 Initial diagnostics...")
        
        # Test basic connectivity
        try:
            health_response = session.get(f"{BASE_URL}/health", timeout=5)
            print(f"✅ Server health check: {health_response.status_code}")
        except Exception as e:
            print(f"⚠️  Server health check failed: {e}")
        
        # 1. Get agent system status
        print("\n📊 Checking agent system status...")
        system_status = get_agent_system_status(session, BASE_URL)
          # 2. Get available agents
        print("\n🤖 Getting available agents...")
        available_agents = get_available_agents(session, BASE_URL)
        
        if not available_agents:
            print("❌ No agents found. Cannot proceed with testing.")
            print("ℹ️  Make sure agents are configured and running on the server.")
            return
        
        # 3. Select an agent to test
        print(f"\n🎯 Selecting agent for testing...")
        # Prefer specific test agents by name, then fall back to any available agent
        preferred_agent_names = ["response-test-agent", "research_assistant", "code_assistant", "content_creator"]
        selected_agent = None
        
        # First try to find by preferred name
        for preferred_name in preferred_agent_names:
            for agent in available_agents:
                if agent['name'].lower() == preferred_name.lower():
                    selected_agent = agent
                    break
            if selected_agent:
                break
          # If no preferred agent found, use the first available agent
        if not selected_agent:
            selected_agent = available_agents[0]
        
        print(f"✅ Selected agent for testing:")
        print(f"   Name: {selected_agent['name']}")
        print(f"   UUID: {selected_agent['uuid']}")
        print(f"   ID: {selected_agent['id']}")
        print(f"   Using identifier: {selected_agent['identifier']}")
        
        selected_agent_id = selected_agent['identifier']
        
        # 3.5. Get detailed information about the selected agent
        print(f"\n🔍 Getting detailed agent information...")
        agent_details = get_agent_by_id(session, BASE_URL, selected_agent_id)
        
        # 4. Test different types of messages with the selected agent
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
                },
                "description": "Simple explanation request"
            }
        ]
        
        print(f"\n💬 Testing agent responses with selected agent: {selected_agent_id}")
        print("=" * 60)
        
        # Use the correct API endpoint for agent execution
        chat_endpoint = f"/api/v1/agents/{selected_agent_id}/execute"
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
            
            try:
                print(f"  Trying endpoint: {chat_endpoint}")
                
                # Execute the agent function
                url = f"{BASE_URL}{chat_endpoint}"
                response = session.post(url, json=request_data, timeout=90)
                
                # Log the request-response
                log_request_response(f"Agent Execute Test {i}", url, request_data, response)
                
                print(f"  Status Code: {response.status_code}")
                
                if response.status_code == 200:
                    try:
                        json_response = response.json()
                        print(f"  ✅ Success! Agent Response:")
                        
                        # Extract the actual response content based on agent API structure
                        if 'data' in json_response:
                            data = json_response['data']
                            if 'result' in data:
                                result_data = data['result']
                                if isinstance(result_data, dict):
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
                    except json.JSONDecodeError:
                        if response.text.strip():
                            print(f"  ✅ Success! Agent Response (text):")
                            print(f"     {response.text}")
                            successful_responses += 1
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
        # 5. Clean up - no cleanup needed since we're using existing agents
        print(f"\n✅ Test completed with agent:")
        if selected_agent_id:
            print(f"   Agent ID/UUID: {selected_agent_id}")
            if 'selected_agent' in locals():
                print(f"   Agent Name: {selected_agent.get('name', 'unknown')}")
        else:
            print("   Agent: unknown")
        print("ℹ️  No cleanup needed - used existing agents")

def main():
    print("🚀 Agent Response Generation Test with Existing Agents")
    print("=" * 70)
    print("This test will:")
    print("1. Get available agents from the server")  
    print("2. Select an appropriate agent for testing")
    print("3. Send various messages to test response generation")
    print("4. Display results and statistics")
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
      # Run the test with existing agents
    test_agent_response_generation()
    
    print("\n🏁 Agent test completed!")
    print("=" * 70)

if __name__ == "__main__":
    main()

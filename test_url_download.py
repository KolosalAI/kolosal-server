#!/usr/bin/env python3
"""
Test script for URL download functionality in the add engine route.
This script tests various scenarios including valid URLs, invalid URLs, and error conditions.
"""

import requests
import json
import time
import os

# Configuration
BASE_URL = "http://localhost:8080"
ENGINES_ENDPOINT = f"{BASE_URL}/v1/engines"

def test_url_download():
    """Test the URL download functionality"""
    
    print("Testing URL download functionality...")
    
    # Test 1: Valid small model URL (using a publicly available small model for testing)
    test_cases = [
        {
            "name": "Valid URL - Small Test Model",
            "payload": {
                "engine_id": "test-url-engine",
                "model_path": "https://huggingface.co/kolosal/qwen3-0.6b/resolve/main/Qwen3-0.6B-UD-Q4_K_XL.gguf",
                "load_immediately": False,
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
            },
            "expected_status": 200
        },
        {
            "name": "Invalid URL",
            "payload": {
                "engine_id": "invalid-url-engine", 
                "model_path": "https://invalid-domain-that-does-not-exist.com/model.gguf",
                "load_immediately": False,
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
            },
            "expected_status": 422
        },
        {
            "name": "Local file path (existing functionality)",
            "payload": {
                "engine_id": "local-file-engine",
                "model_path": "./models/test.gguf",  # This file doesn't exist, should fail
                "load_immediately": False,
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
            },
            "expected_status": 400
        }
    ]
    
    for i, test_case in enumerate(test_cases, 1):
        print(f"\n--- Test {i}: {test_case['name']} ---")
        
        try:
            # Send request
            response = requests.post(
                ENGINES_ENDPOINT,
                headers={"Content-Type": "application/json"},
                json=test_case["payload"],
                timeout=60  # Longer timeout for downloads
            )
            
            print(f"Status Code: {response.status_code}")
            print(f"Expected: {test_case['expected_status']}")
            
            # Parse response
            try:
                response_data = response.json()
                print(f"Response: {json.dumps(response_data, indent=2)}")
            except json.JSONDecodeError:
                print(f"Raw Response: {response.text}")
            
            # Check if status matches expected
            if response.status_code == test_case["expected_status"]:
                print("✅ Test PASSED")
            else:
                print("❌ Test FAILED")
                
        except requests.exceptions.RequestException as e:
            print(f"❌ Request failed: {e}")
        except Exception as e:
            print(f"❌ Unexpected error: {e}")
        
        print("-" * 50)

def check_server_status():
    """Check if the server is running"""
    try:
        response = requests.get(f"{BASE_URL}/health", timeout=5)
        return response.status_code == 200
    except:
        return False

if __name__ == "__main__":
    print("Kolosal Server URL Download Test")
    print("=" * 50)
    
    # Check server status
    if not check_server_status():
        print("❌ Server is not running or not accessible at", BASE_URL)
        print("Please start the server first with: ./kolosal-server")
        exit(1)
    
    print("✅ Server is running")
    
    # Run tests
    test_url_download()
    
    print("\n" + "=" * 50)
    print("Test completed!")

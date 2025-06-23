#!/usr/bin/env python3
import requests
import json

def test_agent_execution():
    base_url = "http://localhost:8080"
    agent_id = "response_test_agent"  # From the list agents response
    
    # Test data
    data = {
        "function": "text_processing",
        "parameters": {
            "text": "Hello World",
            "operation": "analyze"
        }
    }
    
    try:
        # Test with agent ID
        url = f"{base_url}/api/v1/agents/{agent_id}/execute"
        print(f"Testing URL: {url}")
        print(f"Request data: {json.dumps(data, indent=2)}")
        
        response = requests.post(url, json=data)
        print(f"Status: {response.status_code}")
        print(f"Response: {response.text}")
        
        if response.status_code != 404:
            print("✅ Agent found using ID!")
        else:
            print("❌ Still getting 404 with agent ID")
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_agent_execution()

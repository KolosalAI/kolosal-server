#!/usr/bin/env python3
import requests
import json

def test_different_functions():
    """Test different functions to see which ones work"""
    base_url = "http://localhost:8080"
    agent_id = "a808180f-20ba-4fab-9ee7-af6779cd9792"
    
    test_functions = [
        {
            "name": "text_processing",
            "parameters": {"text": "Hello", "operation": "analyze"}
        },
        {
            "name": "code_generation", 
            "parameters": {"requirements": "Python function to add two numbers"}
        },
        {
            "name": "add",  # Basic builtin function
            "parameters": {"a": 5, "b": 3}
        },
        {
            "name": "echo",  # Basic builtin function
            "parameters": {"message": "Hello World"}
        }
    ]
    
    for func_test in test_functions:
        print(f"\n--- Testing function: {func_test['name']} ---")
        data = {
            "function": func_test["name"],
            "parameters": func_test["parameters"]
        }
        
        try:
            url = f"{base_url}/api/v1/agents/{agent_id}/execute"
            response = requests.post(url, json=data)
            
            print(f"Status: {response.status_code}")
            result = response.json()
            print(f"Response: {json.dumps(result, indent=2)}")
            
            if response.status_code == 200:
                print(f"✅ Function '{func_test['name']}' works!")
            else:
                print(f"❌ Function '{func_test['name']}' failed")
                
        except Exception as e:
            print(f"❌ Error testing function '{func_test['name']}': {e}")

if __name__ == "__main__":
    test_different_functions()

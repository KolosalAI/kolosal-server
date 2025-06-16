#!/usr/bin/env python3
"""
Test script to verify the updated metrics endpoints behavior.
Tests that:
1. /metrics returns combined system and completion metrics
2. /v1/metrics returns combined system and completion metrics  
3. /system/metrics returns system metrics only
"""

import requests
import json
import sys

def test_endpoint(url, endpoint_name, should_have_completion_metrics=True, endpoint_type="combined"):
    """Test a specific metrics endpoint"""
    print(f"\n--- Testing {endpoint_name} ---")
    
    try:
        response = requests.get(url, timeout=10)
        print(f"Status Code: {response.status_code}")
        
        if response.status_code != 200:
            print(f"❌ ERROR: Expected status 200, got {response.status_code}")
            print(f"Response: {response.text}")
            return False
            
        data = response.json()
        print(f"Response structure: {list(data.keys())}")
        
        # Check for system metrics (should be present in combined/system endpoints)
        has_system_metrics = 'system_metrics' in data or 'cpu' in data or 'system' in data
        
        # Check for completion metrics 
        has_completion_metrics = 'completion_metrics' in data or 'completion' in data
        
        print(f"Has system metrics: {has_system_metrics}")
        print(f"Has completion metrics: {has_completion_metrics}")
        
        if endpoint_type == "combined":
            if has_system_metrics and has_completion_metrics:
                print(f"✅ SUCCESS: {endpoint_name} returns combined metrics as expected")
                return True
            else:
                print(f"❌ ERROR: {endpoint_name} should return combined metrics")
                return False
        elif endpoint_type == "system":
            if has_system_metrics and not has_completion_metrics:
                print(f"✅ SUCCESS: {endpoint_name} returns system metrics only as expected")
                return True
            else:
                print(f"❌ ERROR: {endpoint_name} should return system metrics only")
                return False
        elif endpoint_type == "completion":
            if has_completion_metrics and not has_system_metrics:
                print(f"✅ SUCCESS: {endpoint_name} returns completion metrics only as expected")
                return True
            else:
                print(f"❌ ERROR: {endpoint_name} should return completion metrics only")
                return False
                print(f"✅ SUCCESS: {endpoint_name} returns combined metrics as expected")
                return True
            else:
                print(f"❌ ERROR: {endpoint_name} should return combined metrics")
                return False
        else:
            if has_system_metrics and not has_completion_metrics:
                print(f"✅ SUCCESS: {endpoint_name} returns system metrics only as expected")
                return True
            else:
                print(f"❌ ERROR: {endpoint_name} should return system metrics only")
                return False
                
    except Exception as e:
        print(f"❌ ERROR: Failed to test {endpoint_name}: {e}")
        return False

def main():
    base_url = "http://localhost:8080"
    
    print("Testing Kolosal Server Metrics Endpoints")
    print("=========================================")    # Test endpoints
    tests = [
        (f"{base_url}/metrics", "/metrics", True, "combined"),           # Should have combined metrics
        (f"{base_url}/v1/metrics", "/v1/metrics", True, "combined"),     # Should have combined metrics  
        (f"{base_url}/metrics/system", "/metrics/system", False, "system"),  # Should have system metrics only
        (f"{base_url}/v1/metrics/system", "/v1/metrics/system", False, "system"),  # Should have system metrics only
        (f"{base_url}/metrics/completion", "/metrics/completion", True, "completion"),  # Should have completion metrics only
        (f"{base_url}/v1/metrics/completion", "/v1/metrics/completion", True, "completion"),  # Should have completion metrics only
    ]
    
    results = []
    for url, name, should_have_completion, endpoint_type in tests:
        result = test_endpoint(url, name, should_have_completion, endpoint_type)
        results.append((name, result))
    
    # Summary
    print("\n" + "="*50)
    print("SUMMARY")
    print("="*50)
    
    passed = 0
    for name, result in results:
        status = "✅ PASS" if result else "❌ FAIL"
        print(f"{name:20} {status}")
        if result:
            passed += 1
    
    print(f"\nPassed: {passed}/{len(results)}")
    
    if passed == len(results):
        print("🎉 All tests passed!")
        sys.exit(0)
    else:
        print("💥 Some tests failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Test script for API key authentication with Kolosal Server.

This script demonstrates how to:
1. Test API key authentication with valid and invalid keys
2. Configure API key settings via the admin API
3. Handle authentication errors gracefully

Requirements:
    pip install requests

Usage:
    python test_api_key_auth.py
"""

import requests
import json
import sys
from typing import Dict, Any


class KolosalAPIKeyTester:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url.rstrip('/')
        self.valid_api_key = "sk-1234567890abcdef"
        self.invalid_api_key = "sk-invalid-key"
        
    def test_without_api_key(self) -> bool:
        """Test request without API key (should fail if required)."""
        print("🔍 Testing request without API key...")
        try:
            response = requests.post(
                f"{self.base_url}/v1/chat/completions",
                json={
                    "messages": [{"role": "user", "content": "Hello"}],
                    "model": "qwen3-0.6b"
                },
                headers={"Content-Type": "application/json"}
            )
            
            if response.status_code == 401:
                print("✅ Correctly rejected request without API key")
                return True
            else:
                print(f"❌ Unexpected response: {response.status_code}")
                print(f"Response: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"❌ Request failed: {e}")
            return False
    
    def test_with_invalid_api_key(self) -> bool:
        """Test request with invalid API key."""
        print("🔍 Testing request with invalid API key...")
        try:
            response = requests.post(
                f"{self.base_url}/v1/chat/completions",
                json={
                    "messages": [{"role": "user", "content": "Hello"}],
                    "model": "qwen3-0.6b"
                },
                headers={
                    "Content-Type": "application/json",
                    "X-API-Key": self.invalid_api_key
                }
            )
            
            if response.status_code == 401:
                print("✅ Correctly rejected request with invalid API key")
                return True
            else:
                print(f"❌ Unexpected response: {response.status_code}")
                print(f"Response: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"❌ Request failed: {e}")
            return False
    
    def test_with_valid_api_key(self) -> bool:
        """Test request with valid API key."""
        print("🔍 Testing request with valid API key...")
        try:
            response = requests.post(
                f"{self.base_url}/v1/chat/completions",
                json={
                    "messages": [{"role": "user", "content": "Hello"}],
                    "model": "qwen3-0.6b"
                },
                headers={
                    "Content-Type": "application/json",
                    "X-API-Key": self.valid_api_key
                }
            )
            
            if response.status_code == 200:
                print("✅ Successfully authenticated with valid API key")
                return True
            elif response.status_code == 401:
                print("❌ Valid API key was rejected - check configuration")
                return False
            else:
                print(f"❌ Unexpected response: {response.status_code}")
                print(f"Response: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"❌ Request failed: {e}")
            return False
    
    def test_with_bearer_token(self) -> bool:
        """Test request with Bearer token format."""
        print("🔍 Testing request with Bearer token...")
        try:
            response = requests.post(
                f"{self.base_url}/v1/chat/completions",
                json={
                    "messages": [{"role": "user", "content": "Hello"}],
                    "model": "qwen3-0.6b"
                },
                headers={
                    "Content-Type": "application/json",
                    "Authorization": f"Bearer {self.valid_api_key}"
                }
            )
            
            if response.status_code == 200:
                print("✅ Successfully authenticated with Bearer token")
                return True
            elif response.status_code == 401:
                print("❌ Bearer token was rejected - check if Authorization header is configured")
                return False
            else:
                print(f"❌ Unexpected response: {response.status_code}")
                print(f"Response: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"❌ Request failed: {e}")
            return False
    
    def get_auth_config(self) -> Dict[str, Any]:
        """Get current authentication configuration."""
        print("🔍 Getting authentication configuration...")
        try:
            response = requests.get(f"{self.base_url}/v1/auth/config")
            
            if response.status_code == 200:
                config = response.json()
                print("✅ Successfully retrieved auth config")
                print(f"API Key Auth: {'enabled' if config.get('api_key', {}).get('enabled') else 'disabled'}")
                print(f"API Key Required: {'yes' if config.get('api_key', {}).get('required') else 'no'}")
                print(f"Header Name: {config.get('api_key', {}).get('header_name', 'N/A')}")
                print(f"Keys Count: {config.get('api_key', {}).get('keys_count', 0)}")
                return config
            else:
                print(f"❌ Failed to get config: {response.status_code}")
                return {}
                
        except requests.RequestException as e:
            print(f"❌ Request failed: {e}")
            return {}
    
    def configure_api_key_auth(self, enabled: bool = True, required: bool = True) -> bool:
        """Configure API key authentication settings."""
        print(f"🔧 Configuring API key auth (enabled: {enabled}, required: {required})...")
        try:
            config = {
                "api_key": {
                    "enabled": enabled,
                    "required": required,
                    "header_name": "X-API-Key",
                    "api_keys": [self.valid_api_key, "sk-another-valid-key"]
                }
            }
            
            response = requests.put(
                f"{self.base_url}/v1/auth/config",
                json=config,
                headers={"Content-Type": "application/json"}
            )
            
            if response.status_code == 200:
                print("✅ Successfully updated API key configuration")
                return True
            else:
                print(f"❌ Failed to update config: {response.status_code}")
                print(f"Response: {response.text}")
                return False
                
        except requests.RequestException as e:
            print(f"❌ Request failed: {e}")
            return False
    
    def run_tests(self):
        """Run all authentication tests."""
        print("🚀 Starting API Key Authentication Tests for Kolosal Server")
        print("=" * 60)
        
        # Check if server is running
        try:
            response = requests.get(f"{self.base_url}/v1/health")
            if response.status_code != 200:
                print("❌ Server is not running or health check failed")
                return False
        except requests.RequestException:
            print("❌ Cannot connect to server. Make sure it's running on the configured port.")
            return False
        
        print("✅ Server is running and reachable")
        print()
        
        # Get current configuration
        current_config = self.get_auth_config()
        print()
        
        # Configure API key authentication
        if not self.configure_api_key_auth():
            print("❌ Failed to configure API key authentication")
            return False
        print()
        
        # Run authentication tests
        test_results = []
        
        test_results.append(self.test_without_api_key())
        print()
        
        test_results.append(self.test_with_invalid_api_key())
        print()
        
        test_results.append(self.test_with_valid_api_key())
        print()
        
        test_results.append(self.test_with_bearer_token())
        print()
        
        # Summary
        passed = sum(test_results)
        total = len(test_results)
        
        print("📊 Test Results Summary")
        print("=" * 30)
        print(f"Passed: {passed}/{total}")
        print(f"Failed: {total - passed}/{total}")
        
        if passed == total:
            print("🎉 All tests passed! API key authentication is working correctly.")
        else:
            print("⚠️  Some tests failed. Check the configuration and server logs.")
        
        return passed == total


def main():
    """Main function to run the API key authentication tests."""
    import argparse
    
    parser = argparse.ArgumentParser(description="Test API key authentication for Kolosal Server")
    parser.add_argument("--url", default="http://localhost:8080", 
                       help="Base URL of the Kolosal Server (default: http://localhost:8080)")
    parser.add_argument("--api-key", default="sk-1234567890abcdef",
                       help="Valid API key to use for testing")
    
    args = parser.parse_args()
    
    tester = KolosalAPIKeyTester(args.url)
    tester.valid_api_key = args.api_key
    
    success = tester.run_tests()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()

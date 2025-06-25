#!/usr/bin/env python3
"""
Test script to demonstrate the enhanced logging functionality
"""

import json
from sequential_workflow_demo import SequentialWorkflowDemo

def test_logging():
    """Test the logging functionality"""
    demo = SequentialWorkflowDemo()
    
    # Test the new logging methods
    demo.log_workflow_content("test_workflow", "This is sample generated content", "step1", "TEST_CONTENT")
    
    # Test result logging
    sample_result = {
        "workflow_id": "test_workflow",
        "success": True,
        "total_steps": 2,
        "total_execution_time_ms": 1500.5,
        "step_results": {
            "step1": {
                "success": True,
                "result_data": {
                    "text": "This is the generated text from step 1",
                    "summary": "A brief summary of the results"
                }
            },
            "step2": {
                "success": True,
                "result_data": {
                    "analysis": "This is the analysis output from step 2",
                    "recommendations": "These are the recommendations"
                }
            }
        }
    }
    
    demo.log_workflow_results("test_workflow", sample_result)
    print("✅ Logging test completed. Check the log file for detailed output.")

if __name__ == "__main__":
    test_logging()

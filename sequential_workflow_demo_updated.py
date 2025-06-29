#!/usr/bin/env python3
"""
Updated Sequential Workflow Demo for Kolosal Server

This updated demo showcases the simplified workflow system with automatic setup
and real-time streaming capabilities. Compared to the original complex demo, 
this version demonstrates:

1. Zero-configuration setup (no manual agent mapping needed)
2. Automatic model/engine detection and setup
3. Simplified workflow creation and execution
4. Built-in error handling and retries
5. Real-world workflow patterns made easy
6. Real-time streaming execution with live output
7. Interactive user experience with immediate feedback

Features demonstrated:
- Content creation workflows (research → write → review)
- Code development workflows (design → implement → test)  
- Data analysis workflows (collect → analyze → report)
- Multi-step agent collaboration
- Workflow monitoring and progress tracking
- Error handling and recovery
- Real-time streaming execution
- Streaming vs non-streaming comparisons
- Interactive workflow creation

Usage:
    python sequential_workflow_demo_updated.py [--host HOST] [--port PORT] [--interactive]
    python sequential_workflow_demo_updated.py --demo streaming  # Show streaming capabilities
"""

import sys
import argparse
import json
import time
import logging
from typing import Dict, Any, Optional, List
from datetime import datetime

# Import the simplified client
from simple_workflow_client import SimpleWorkflowClient, quick_content_creation, quick_code_generation

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('sequential_workflow_demo_updated.log', encoding='utf-8'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

class UpdatedSequentialWorkflowDemo:
    """Updated demo using the simplified workflow system"""
    
    def __init__(self, host: str = "localhost", port: int = 8080, debug: bool = False):
        self.host = host
        self.port = port
        self.debug = debug  # Add debug flag
        
        print("🚀 Initializing Updated Sequential Workflow Demo")
        print("="*60)
        
        # Create client - auto-setup handles everything!
        print("📡 Connecting to server and auto-setting up...")
        self.client = SimpleWorkflowClient(host=host, port=port)
        
        if not self.client.check_server_health():
            raise Exception(f"❌ Server not available at {host}:{port}")
            
        print("✅ Connected successfully!")
        print("✅ Auto-setup completed automatically!")
        
        # Get available agents for validation
        self.available_agents = self.client.get_available_agents()
        print(f"📋 Available agents: {', '.join(self.available_agents)}")
        
        if debug:
            print("🐛 Debug mode enabled - will show detailed streaming information")
        
    def _validate_agent_name(self, agent_name: str) -> str:
        """Validate and suggest correct agent name if needed"""
        if agent_name in self.available_agents:
            return agent_name
            
        # Map common agent names to available ones
        agent_mapping = {
            'system_architect': 'code_assistant',
            'code_generator': 'code_assistant', 
            'frontend_developer': 'code_assistant',
            'backend_developer': 'code_assistant',
            'devops_engineer': 'code_assistant',
            'technical_writer': 'content_creator',
            'ui_designer': 'content_creator',
            'creative_writer': 'content_creator',
            'editor': 'qa_specialist',
            'reviewer': 'qa_specialist',
            'qa_engineer': 'qa_specialist',
            'product_manager': 'project_manager',
            'business_analyst': 'research_assistant',
            'data_engineer': 'data_analyst',
            'data_collector': 'data_analyst',
            'data_visualizer': 'data_analyst',
            'site_reliability_engineer': 'code_assistant'
        }
        
        if agent_name in agent_mapping:
            mapped_name = agent_mapping[agent_name]
            print(f"   🔄 Mapping '{agent_name}' -> '{mapped_name}'")
            return mapped_name
            
        # If no mapping found, use a default
        print(f"   ⚠️  Agent '{agent_name}' not found, using 'research_assistant' as default")
        return 'research_assistant'
        
    def _debug_workflow_creation(self, workflow):
        """Debug helper to show workflow details"""
        print(f"   🔍 Workflow Debug Info:")
        print(f"     • ID: {workflow.workflow_id}")
        print(f"     • Name: {workflow.workflow_name}")
        print(f"     • Steps: {len(workflow.steps)}")
        
        for i, step in enumerate(workflow.steps):
            agent_valid = step.agent_name in self.available_agents
            status = "✅" if agent_valid else "❌"
            print(f"     • Step {i+1}: {step.step_id} -> {step.agent_name} {status}")
            
        return workflow
        
    def _execute_workflow_with_enhanced_output(self, workflow, streaming=True, show_step_outputs=True):
        """Execute workflow with enhanced output display and real-time LLM streaming"""
        if not streaming:
            # For non-streaming, just execute normally
            result = self.client.execute_workflow(workflow, streaming=False)
            if result and show_step_outputs:
                self._display_step_by_step_results(result)
            return result
        
        print("   🌊 Starting streaming execution...")
        print("   " + "─" * 60)
        
        # Execute with enhanced streaming display
        try:
            result = self._execute_with_enhanced_streaming(workflow)
            print("   " + "─" * 60)
            
            # If streaming failed, try non-streaming as fallback
            if result is None:
                print("   🔄 Streaming failed, trying non-streaming fallback...")
                result = self.client.execute_workflow(workflow, streaming=False)
                if result:
                    print("   ✅ Non-streaming fallback successful!")
                else:
                    print("   ❌ Both streaming and non-streaming failed!")
            
            if result and show_step_outputs:
                print("   📝 Final Step Summary:")
                self._display_step_by_step_results(result)
                        
            return result
            
        except Exception as e:
            print("   " + "─" * 60)
            print(f"   ❌ Streaming execution failed: {e}")
            logger.error(f"Enhanced streaming execution error: {e}")
            
            # Try non-streaming as fallback
            print("   🔄 Attempting non-streaming fallback...")
            try:
                result = self.client.execute_workflow(workflow, streaming=False)
                if result:
                    print("   ✅ Non-streaming fallback successful!")
                    if show_step_outputs:
                        self._display_step_by_step_results(result)
                    return result
                else:
                    print("   ❌ Non-streaming fallback also failed!")
            except Exception as fallback_error:
                print(f"   ❌ Fallback execution failed: {fallback_error}")
            
            return None
            
    def _execute_with_enhanced_streaming(self, workflow):
        """Execute workflow with enhanced real-time streaming display"""
        try:
            # Convert to API format 
            api_workflow = self.client._convert_workflow_to_api(workflow)
            if not api_workflow:
                print("❌ Failed to convert workflow - missing agents")
                return None
            
            # Register workflow
            if not self.client._register_workflow_with_cleanup(api_workflow):
                print(f"❌ Failed to register workflow: {workflow.workflow_id}")
                return None
            
            # Execute with custom streaming handler
            return self._stream_workflow_execution(workflow.workflow_id)
            
        except Exception as e:
            logger.error(f"Enhanced streaming execution failed: {e}")
            return None
    
    def _stream_workflow_execution(self, workflow_id: str):
        """Execute workflow with enhanced streaming output"""
        import requests
        import json
        import time
        
        try:
            print(f"   📡 Starting streaming execution for workflow: {workflow_id}")
            
            response = self.client.session.post(
                f"{self.client.base_url}/api/v1/sequential-workflows/{workflow_id}/execute",
                json={},
                headers={
                    "Content-Type": "application/json", 
                    "Accept": "text/event-stream"
                },
                stream=True,
                timeout=300  # 5 minute timeout
            )
            
            print(f"   📡 Response status: {response.status_code}")
            print(f"   📡 Response headers: {dict(response.headers)}")
            
            if response.status_code != 200:
                print(f"❌ Streaming execution failed: {response.status_code}")
                # Try to read error response
                try:
                    error_text = response.text
                    print(f"   Error details: {error_text}")
                except:
                    pass
                return None
            
            # Check if server actually supports streaming
            content_type = response.headers.get('content-type', '').lower()
            is_sse = 'text/event-stream' in content_type
            
            if not is_sse:
                print(f"   ⚠️  Server returned Content-Type: {content_type}")
                print(f"   ⚠️  Server does not support streaming for workflows - falling back to JSON parsing")
                
                # Server returned regular JSON instead of SSE
                try:
                    response_text = response.text
                    if response_text.strip():
                        result_json = json.loads(response_text)
                        print(f"   ✅ Received complete JSON response ({len(response_text)} characters)")
                        
                        # Extract the actual result data
                        if 'data' in result_json:
                            final_result = result_json['data']
                        else:
                            final_result = result_json
                            
                        # Display the final output immediately since we have it all
                        if final_result and 'final_output' in final_result:
                            print(f"\n   📺 Complete Workflow Output:")
                            print(f"   " + "═" * 60)
                            output_text = str(final_result['final_output'])
                            # Show the output with proper formatting
                            output_lines = output_text.split('\n')
                            for i, line in enumerate(output_lines[:20], 1):
                                print(f"   {i:3d} │ {line}")
                            if len(output_lines) > 20:
                                total_lines = len(output_lines)
                                print(f"   ... │ (showing first 20 lines of {total_lines} total)")
                        
                        # Always show the complete response JSON as well
                        self._print_full_response_json(final_result, "Non-Streaming Workflow")
                            
                        return final_result
                    else:
                        print(f"   ❌ Empty response from server")
                        return None
                except json.JSONDecodeError as e:
                    print(f"   ❌ Failed to parse JSON response: {e}")
                    print(f"   Raw response: {response_text[:500]}...")
                    return None
            
            # Server supports streaming - proceed with SSE parsing
            print(f"   ✅ Server supports streaming (Content-Type: {content_type})")
            
            # Parse streaming response with enhanced display
            current_step = None
            current_step_output = ""
            buffer = ""
            final_result = None
            step_counter = 0
            chunks_received = 0
            
            print("   📺 Live LLM Output Stream:")
            print("   " + "═" * 60)
            
            for chunk in response.iter_content(chunk_size=512, decode_unicode=True):
                if not chunk:
                    continue
                    
                chunks_received += 1
                buffer += chunk
                
                # Debug: Show first few chunks to understand format
                if self.debug and chunks_received <= 3:
                    print(f"   DEBUG: Chunk {chunks_received}: {repr(chunk[:100])}")
                
                # Process complete lines
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    
                    if line.startswith('data:'):
                        data_content = line[5:].strip()
                        
                        # Debug: Show what data we're getting
                        if self.debug and chunks_received <= 5:
                            print(f"   DEBUG: Data line: {data_content[:200]}")
                        
                        # Try to parse as JSON (structured data)
                        try:
                            event_data = json.loads(data_content)
                            
                            # Handle different event types
                            if isinstance(event_data, dict):
                                event_type = event_data.get('type', 'unknown')
                                
                                if event_type == 'step_start':
                                    step_counter += 1
                                    step_name = event_data.get('step_name', f'Step {step_counter}')
                                    agent_name = event_data.get('agent_name', 'unknown')
                                    current_step = step_name
                                    current_step_output = ""
                                    print(f"\n   🎯 Step {step_counter}: {step_name}")
                                    print(f"   👤 Agent: {agent_name}")
                                    print(f"   💭 LLM Output:")
                                    
                                elif event_type == 'step_complete':
                                    if current_step:
                                        success = event_data.get('success', False)
                                        icon = "✅" if success else "❌"
                                        print(f"\n   {icon} {current_step} completed")
                                        if not success and 'error' in event_data:
                                            print(f"   ❌ Error: {event_data['error']}")
                                    current_step = None
                                    print("   " + "─" * 50)
                                    
                                elif event_type == 'workflow_complete':
                                    final_result = event_data.get('result', event_data)
                                    print(f"\n   🏁 Workflow completed successfully!")
                                    
                                elif event_type == 'llm_token' or event_type == 'token':
                                    # Real-time token streaming
                                    token = event_data.get('token', event_data.get('content', ''))
                                    if token:
                                        print(token, end='', flush=True)
                                        current_step_output += token
                                        
                                elif event_type == 'llm_output' or event_type == 'output':
                                    # Chunk of LLM output
                                    output = event_data.get('output', event_data.get('content', ''))
                                    if output:
                                        print(output, end='', flush=True) 
                                        current_step_output += output
                                        
                                elif event_type == 'error':
                                    error_msg = event_data.get('message', str(event_data))
                                    print(f"\n   ❌ Error: {error_msg}")
                                    
                                # If it's a complete result object
                                elif 'final_output' in event_data or 'step_results' in event_data:
                                    final_result = event_data
                                    print(f"\n   📋 Complete result received via streaming")
                                    
                        except json.JSONDecodeError:
                            # Handle plain text streaming (raw LLM output)
                            if data_content and data_content != '[DONE]':
                                if self.debug and chunks_received <= 5:
                                    print(f"   DEBUG: Raw text: {data_content}")
                                print(data_content, end='', flush=True)
                                current_step_output += data_content
                                
                    elif line.startswith('event:'):
                        event_type = line[6:].strip()
                        if self.debug and chunks_received <= 5:
                            print(f"   DEBUG: Event type: {event_type}")
                                
                    elif line and not line.startswith(':'):
                        # Handle any other text output
                        if self.debug and chunks_received <= 5:
                            print(f"   DEBUG: Other line: {line[:100]}")
                        print(line, end='', flush=True)
            
            if self.debug:
                print(f"\n   DEBUG: Total chunks received: {chunks_received}")
            print("   " + "═" * 60)
            print("   ✅ Streaming completed")
            
            # If no result was captured from streaming, try to get it via regular API
            if final_result is None:
                print("   ⚠️  No result from streaming, attempting fallback...")
                try:
                    # Try to get workflow result via status endpoint
                    status_response = self.client.session.get(
                        f"{self.client.base_url}/api/v1/sequential-workflows/{workflow_id}/status"
                    )
                    if status_response.status_code == 200:
                        status_data = status_response.json()
                        if status_data.get('status') == 'completed':
                            final_result = status_data.get('result', status_data)
                            print("   ✅ Retrieved result via fallback")
                    
                    # If still no result, try executing non-streaming as fallback
                    if final_result is None:
                        print("   🔄 Falling back to non-streaming execution...")
                        fallback_response = self.client.session.post(
                            f"{self.client.base_url}/api/v1/sequential-workflows/{workflow_id}/execute",
                            json={},
                            headers={"Content-Type": "application/json"},
                            timeout=300
                        )
                        if fallback_response.status_code == 200:
                            fallback_json = fallback_response.json()
                            if 'data' in fallback_json:
                                final_result = fallback_json['data']
                            else:
                                final_result = fallback_json
                            print("   ✅ Fallback execution successful")
                            # Show the complete JSON response from fallback
                            self._print_full_response_json(final_result, "Fallback Non-Streaming")
                        else:
                            print(f"   ❌ Fallback execution failed: {fallback_response.status_code}")
                            
                except Exception as fallback_error:
                    print(f"   ❌ Fallback failed: {fallback_error}")
            
            # Show complete response JSON for streaming results too
            if final_result:
                self._print_full_response_json(final_result, "Streaming Workflow")
            
            return final_result
            
        except Exception as e:
            logger.error(f"Enhanced streaming failed: {e}")
            print(f"\n   ❌ Streaming failed: {e}")
            return None
            
    def _display_step_by_step_results(self, result):
        """Display detailed step-by-step results"""
        step_results = result.get('step_results', {})
        if not step_results:
            return
            
        print("   📊 Detailed Step Results:")
        for i, (step_name, step_data) in enumerate(step_results.items(), 1):
            success_icon = "✅" if step_data.get('success') else "❌"
            print(f"   {i}. {success_icon} {step_name}")
            
            if step_data.get('success') and step_data.get('output'):
                output = str(step_data['output'])
                # Show first few lines of each step's output
                lines = output.split('\n')
                for j, line in enumerate(lines[:3]):
                    print(f"      │ {line}")
                if len(lines) > 3:
                    print(f"      │ ... ({len(lines) - 3} more lines)")
            elif not step_data.get('success') and step_data.get('error'):
                print(f"      ❌ Error: {step_data.get('error')}")
            print()
            
    def _show_full_output(self, result: Dict[str, Any], title: str = "Complete LLM Output"):
        """Display the complete, untruncated output of a workflow result with enhanced formatting"""
        if not result:
            print(f"   📭 No {title.lower()} to display")
            return
            
        print(f"\n   � {title}")
        print("   " + "═" * 80)
        
        # Show final output with full detail
        if 'final_output' in result and result['final_output']:
            print("   🎯 Final LLM Output (Complete):")
            print("   " + "─" * 80)
            output = str(result['final_output'])
            lines = output.split('\n')
            
            for i, line in enumerate(lines, 1):
                # Color-code different types of content
                display_line = line.rstrip()
                if line.startswith('#'):
                    print(f"   {i:4d} │ 📝 {display_line}")  # Headers
                elif line.startswith('-') or line.startswith('*'):
                    print(f"   {i:4d} │ • {display_line}")   # Bullet points
                elif line.startswith('```'):
                    print(f"   {i:4d} │ 💻 {display_line}")  # Code blocks
                elif line.strip() == '':
                    print(f"   {i:4d} │")                     # Empty lines
                else:
                    print(f"   {i:4d} │ {display_line}")     # Regular text
        
        # Show detailed step outputs if available
        step_results = result.get('step_results', {})
        if step_results and len(step_results) > 1:
            print("\n   " + "─" * 80)
            print("   📝 Complete Step-by-Step Outputs:")
            
            for step_num, (step_name, step_data) in enumerate(step_results.items(), 1):
                print(f"\n   📍 Step {step_num}: {step_name}")
                print("   " + "·" * 60)
                
                if step_data.get('success') and step_data.get('output'):
                    output = str(step_data['output'])
                    lines = output.split('\n')
                    
                    for i, line in enumerate(lines, 1):
                        display_line = line.rstrip()
                        if display_line:
                            print(f"   {i:4d} │ {display_line}")
                        else:
                            print(f"   {i:4d} │")
                    
                    # Show output statistics
                    char_count = len(output)
                    word_count = len(output.split())
                    line_count = len(lines)
                    print(f"   ℹ️   {line_count} lines, {word_count} words, {char_count} characters")
                    
                elif not step_data.get('success'):
                    error_msg = step_data.get('error', 'Unknown error')
                    print(f"   ❌ Error: {error_msg}")
                else:
                    print(f"   ⚪ No output generated")
        
        # Show execution metadata
        print("\n   " + "─" * 80)
        print("   📊 Execution Metadata:")
        if 'execution_time' in result:
            print(f"   ⏱️  Total execution time: {result['execution_time']:.2f} seconds")
        if 'timestamp' in result:
            print(f"   📅 Executed at: {result['timestamp']}")
        if 'workflow_id' in result:
            print(f"   🆔 Workflow ID: {result['workflow_id']}")
        
        # Show final statistics
        if 'final_output' in result and result['final_output']:
            final_output = str(result['final_output'])
            total_chars = len(final_output)
            total_words = len(final_output.split())
            total_lines = len(final_output.split('\n'))
            
            print(f"\n   📈 Final Output Statistics:")
            print(f"   📏 Total lines: {total_lines}")
            print(f"   📝 Total words: {total_words}")
            print(f"   🔤 Total characters: {total_chars}")
            
            # Estimate reading time (average 200 words per minute)
            reading_time = max(1, total_words / 200)
            print(f"   📖 Estimated reading time: {reading_time:.1f} minutes")
        
        print("   " + "═" * 80)
        
    def show_available_agents(self):
        """Display available agents"""
        print("\n📊 Available Agents:")
        print("-" * 40)
        
        agents = self.client.get_available_agents()
        if not agents:
            print("  ⚠️  No agents available")
            return
            
        for agent in agents:
            print(f"  • {agent}")
            
        print(f"\n📈 Total: {len(agents)} agents ready for use")
        
    def demo_content_creation_workflow(self):
        """Demonstrate a content creation workflow"""
        print("\n" + "="*60)
        print("🎯 DEMO 1: Content Creation Workflow")
        print("="*60)
        
        print("\n📝 Creating a research → write → review workflow")
        
        # Method 1: Use built-in template (ultra-simple)
        print("\n🚀 Method 1: Built-in Template (One-liner)")
        print("   quick_content_creation('The Future of AI in Education', 'educators')")
        
        result = quick_content_creation(
            topic="The Future of AI in Education",
            audience="educators"
        )
        
        if result:
            print("   ✅ Content creation completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Content Creation")
        else:
            print("   ❌ Content creation failed")
            
        # Method 2: Custom workflow with streaming
        print("\n🌊 Method 2: Custom Workflow with Streaming")
        
        workflow = self.client.create_workflow(
            "streaming_content_workflow",
            "Streaming Content Creation Pipeline"
        )
        
        # Add steps using agent names (no UUID mapping needed!)
        workflow.add_research_step("AI in Education trends and applications")
        workflow.add_writing_step("educational article for teachers")
        workflow.add_review_step("review for accuracy and readability")
        
        print("   ✓ Research step added")
        print("   ✓ Writing step added") 
        print("   ✓ Review step added")
        
        print("\n▶️  Executing workflow with real-time streaming...")
        print("   📡 Watch the AI work in real-time:")
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Streaming workflow completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Content Creation Streaming")
        else:
            print("   ❌ Streaming workflow failed - no result returned")
            
        # Method 3: Non-streaming for comparison
        print("\n🛠️  Method 3: Traditional Non-Streaming (For Comparison)")
        
        workflow = self.client.create_workflow(
            "traditional_content_workflow",
            "Traditional Content Creation Pipeline"
        )
        
        workflow.add_research_step("AI in Education trends and applications")
        workflow.add_writing_step("educational article for teachers")
        workflow.add_review_step("review for accuracy and readability")
        
        print("   ✓ Research step added")
        print("   ✓ Writing step added") 
        print("   ✓ Review step added")
        
        print("\n▶️  Executing traditional workflow (no streaming)...")
        result = self.client.execute_workflow(workflow, streaming=False)
        
        if result:
            print("   ✅ Traditional workflow completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Traditional Content Creation")
        else:
            print("   ❌ Traditional workflow failed")
            
    def demo_code_development_workflow(self):
        """Demonstrate a code development workflow"""
        print("\n" + "="*60)
        print("💻 DEMO 2: Code Development Workflow")
        print("="*60)
        
        print("\n🔧 Creating a design → implement → test workflow")
        
        # Method 1: Built-in template with streaming
        print("\n🌊 Method 1: Built-in Template with Streaming")
        print("   Streaming code generation for 'User authentication REST API'")
        
        # Create a client method that supports streaming for built-in templates
        workflow = self.client.create_workflow(
            "streaming_code_workflow",
            "Streaming Code Development"
        )
        
        workflow.add_code_generation_step("User authentication REST API", "python")
        workflow.add_review_step("code quality, security, best practices", self._validate_agent_name("qa_specialist"))
        workflow.add_step(
            "document",
            self._validate_agent_name("content_creator"),
            "Create comprehensive documentation for the API including usage examples",
            temperature=0.3
        )
        
        print("   ✓ Code generation step added")
        print("   ✓ Code review step added")
        print("   ✓ Documentation step added")
        
        # Debug the workflow before execution
        self._debug_workflow_creation(workflow)
        
        print("\n▶️  Executing with real-time streaming...")
        print("   � Watch the code generation in real-time:")
        print("   " + "-" * 50)
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Streaming code development completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Code Development")
        else:
            print("   ❌ Streaming code development failed - no result returned")
            
        # Method 2: Traditional built-in template for comparison
        print("\n�🚀 Method 2: Traditional Built-in Template (No Streaming)")
        print("   quick_code_generation('User authentication REST API')")
        
        result = quick_code_generation("User authentication REST API")
        
        if result:
            print("   ✅ Traditional code generation completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Traditional Code Generation")
        else:
            print("   ❌ Traditional code generation failed")
            
        # Method 3: Custom development workflow with streaming
        print("\n🛠️  Method 3: Custom Development Workflow with Streaming")
        
        workflow = self.client.create_workflow(
            "custom_streaming_dev_workflow", 
            "Full Development Pipeline with Streaming"
        )
        
        # Add steps for full development cycle
        workflow.add_step(
            "design",
            self._validate_agent_name("code_assistant"), 
            "Design architecture for a task management API"
        )
        workflow.add_step(
            "implement",
            self._validate_agent_name("code_assistant"),
            "Implement the task management API based on the design"
        )
        workflow.add_step(
            "test",
            self._validate_agent_name("qa_specialist"),
            "Create comprehensive tests for the API"
        )
        workflow.add_step(
            "document",
            self._validate_agent_name("content_creator"),
            "Write API documentation and usage examples"
        )
        
        print("   ✓ Architecture design step added")
        print("   ✓ Implementation step added")
        print("   ✓ Testing step added")
        print("   ✓ Documentation step added")
        
        # Debug the workflow before execution
        self._debug_workflow_creation(workflow)
        
        print("\n▶️  Executing development workflow with streaming...")
        print("   📡 Real-time development pipeline:")
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Streaming development workflow completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Custom Development Pipeline")
        else:
            print("   ❌ Streaming development workflow failed - no result returned")
            
    def demo_data_analysis_workflow(self):
        """Demonstrate a data analysis workflow"""
        print("\n" + "="*60)
        print("📊 DEMO 3: Data Analysis Workflow")
        print("="*60)
        
        print("\n📈 Creating a collect → analyze → report workflow")
        
        # Method 1: Streaming data analysis
        print("\n🌊 Method 1: Streaming Data Analysis")
        
        workflow = self.client.create_workflow(
            "streaming_data_analysis",
            "Streaming Data Analysis Pipeline"
        )
        
        workflow.add_step(
            "prepare_data",
            "data_analyst", 
            "Prepare and validate customer satisfaction survey responses from Q4 2024",
            function_name="data_analysis",
            parameters={"operation": "data_preparation"}
        )
        
        workflow.add_step(
            "analyze", 
            "data_analyst",
            "Perform statistical analysis on the prepared survey data",
            function_name="data_analysis", 
            parameters={"operation": "statistical_analysis"}
        )
        
        workflow.add_step(
            "generate_insights",
            "research_assistant",
            "Based on the analysis results, generate key insights and actionable recommendations",
            temperature=0.4
        )
        
        print("   ✓ Data preparation step added")
        print("   ✓ Statistical analysis step added")
        print("   ✓ Insights generation step added")
        
        print("\n▶️  Executing data analysis with streaming...")
        print("   � Watch the data analysis in real-time:")
        print("   " + "-" * 50)
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Streaming data analysis completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Data Analysis Streaming")
            self._maybe_show_full_output(result, "Data Analysis Results")
        else:
            print("   ❌ Streaming data analysis failed")
            
        # Method 2: Built-in data analysis template (traditional)
        print("\n🚀 Method 2: Built-in Data Analysis Template (Traditional)")
        
        result = self.client.run_data_analysis_workflow(
            "customer satisfaction survey responses from Q4 2024"
        )
        
        if result:
            print("   ✅ Traditional data analysis completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Traditional Data Analysis")
        else:
            print("   ❌ Traditional data analysis failed")
            
        # Method 3: Advanced analysis workflow with streaming
        print("\n🛠️  Method 3: Advanced Analysis Workflow with Streaming")
        
        workflow = self.client.create_workflow(
            "advanced_streaming_analysis",
            "Advanced Data Analysis Pipeline with Streaming" 
        )
        
        workflow.add_step(
            "collect",
            "data_analyst",
            "Gather sales data from multiple sources for Q4 2024"
        )
        workflow.add_step(
            "clean",
            "data_analyst", 
            "Clean and preprocess the collected sales data"
        )
        workflow.add_step(
            "analyze",
            "data_analyst",
            "Perform statistical analysis and identify trends"
        )
        workflow.add_step(
            "visualize",
            "data_analyst",
            "Create charts and dashboards for the analysis"
        )
        workflow.add_step(
            "report",
            "content_creator",
            "Generate executive summary and recommendations"
        )
        
        print("   ✓ Data collection step added")
        print("   ✓ Data cleaning step added")
        print("   ✓ Analysis step added")
        print("   ✓ Visualization step added")
        print("   ✓ Reporting step added")
        
        print("\n▶️  Executing advanced analysis workflow with streaming...")
        print("   📡 Real-time analysis pipeline:")
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Streaming advanced analysis completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Advanced Data Analysis")
            self._maybe_show_full_output(result, "Advanced Analysis Results")
        else:
            print("   ❌ Streaming advanced analysis failed")
            
    def demo_collaborative_workflow(self):
        """Demonstrate a multi-agent collaborative workflow"""
        print("\n" + "="*60)
        print("🤝 DEMO 4: Multi-Agent Collaborative Workflow")
        print("="*60)
        
        print("\n👥 Creating a collaborative product development workflow")
        
        # Method 1: Streaming collaborative workflow
        print("\n🌊 Method 1: Collaborative Workflow with Streaming")
        
        workflow = self.client.create_workflow(
            "streaming_collaborative_workflow",
            "Streaming Collaborative Product Development"
        )
        
        # Product manager starts
        workflow.add_step(
            "requirements",
            "project_manager",
            "Define requirements for a new mobile app feature"
        )
        
        # Designer creates mockups
        workflow.add_step(
            "design",
            "content_creator", 
            "Create UI/UX design documentation based on the requirements"
        )
        
        # Developer implements
        workflow.add_step(
            "frontend",
            "code_assistant",
            "Implement the frontend based on the design"
        )
        
        workflow.add_step(
            "backend",
            "code_assistant",
            "Create backend API to support the frontend"
        )
        
        # QA tests everything
        workflow.add_step(
            "qa_testing",
            "qa_specialist",
            "Test the complete feature end-to-end"
        )
        
        print("   ✓ Requirements gathering step added")
        print("   ✓ UI/UX design step added")
        print("   ✓ Frontend development step added")
        print("   ✓ Backend development step added")
        print("   ✓ QA testing step added")
        
        print("\n▶️  Executing collaborative workflow with streaming...")
        print("   📡 Watch team collaboration in real-time:")
        print("   " + "-" * 50)
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Streaming collaborative workflow completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Collaborative Workflow")
            self._maybe_show_full_output(result, "Collaborative Development Results")
        else:
            print("   ❌ Streaming collaborative workflow failed")
            
        # Method 2: Extended collaborative workflow with deployment
        print("\n🛠️  Method 2: Extended Collaborative Workflow with Streaming")
        
        workflow = self.client.create_workflow(
            "extended_collaborative_workflow",
            "Full Product Development Lifecycle with Streaming"
        )
        
        # Full product development cycle using available agents
        workflow.add_step(
            "market_research",
            "research_assistant",
            "Conduct market research for the new feature"
        )
        
        workflow.add_step(
            "requirements",
            "project_manager",
            "Define detailed requirements based on market research"
        )
        
        workflow.add_step(
            "architecture",
            "code_assistant",
            "Design system architecture for the feature"
        )
        
        workflow.add_step(
            "design",
            "content_creator", 
            "Create UI/UX design mockups and documentation based on requirements and architecture"
        )
        
        workflow.add_step(
            "frontend",
            "code_assistant",
            "Implement the frontend components"
        )
        
        workflow.add_step(
            "backend",
            "code_assistant",
            "Develop backend services and APIs"
        )
        
        workflow.add_step(
            "testing",
            "qa_specialist",
            "Perform comprehensive testing"
        )
        
        workflow.add_step(
            "deployment",
            "code_assistant", 
            "Create deployment scripts and configuration for production"
        )
        
        workflow.add_step(
            "monitoring",
            "code_assistant",
            "Set up monitoring and alerting scripts for the new feature"
        )
        
        workflow.add_step(
            "review",
            "project_manager",
            "Review the deployed feature and gather initial feedback"
        )
        
        print("   ✓ Market research step added")
        print("   ✓ Requirements gathering step added")
        print("   ✓ Architecture design step added")
        print("   ✓ UI/UX design step added")
        print("   ✓ Frontend development step added")
        print("   ✓ Backend development step added")
        print("   ✓ Testing step added")
        print("   ✓ Deployment step added")
        print("   ✓ Monitoring setup step added")
        print("   ✓ Product review step added")
        
        print("\n▶️  Executing extended collaborative workflow with streaming...")
        print("   📡 Full development lifecycle in real-time:")
        print("   " + "-" * 50)
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Extended streaming collaborative workflow completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Extended Collaborative Workflow")
            self._maybe_show_full_output(result, "Extended Development Lifecycle Results")
        else:
            print("   ❌ Extended streaming collaborative workflow failed")
            
    def demo_workflow_monitoring(self):
        """Demonstrate workflow monitoring and progress tracking"""
        print("\n" + "="*60)
        print("📡 DEMO 5: Workflow Monitoring & Progress Tracking")
        print("="*60)
        
        print("\n⏱️  Creating workflow with progress monitoring")
        
        # Method 1: Streaming workflow with visual progress
        print("\n🌊 Method 1: Real-time Streaming with Progress Visualization")
        
        workflow = self.client.create_workflow(
            "streaming_monitored_workflow",
            "Workflow with Real-time Streaming & Progress"
        )
        
        # Add several steps to demonstrate monitoring
        workflow.add_step("step1", "research_assistant", "Research AI trends in 2024")
        workflow.add_step("step2", "data_analyst", "Analyze research data and identify patterns")
        workflow.add_step("step3", "content_creator", "Create comprehensive summary report")
        workflow.add_step("step4", "qa_specialist", "Review and refine report for publication")
        workflow.add_step("step5", "content_creator", "Format report with proper citations and references")
        
        print("   ✓ Research step added")
        print("   ✓ Analysis step added") 
        print("   ✓ Content creation step added")
        print("   ✓ Review step added")
        print("   ✓ Formatting step added")
        
        print("\n▶️  Executing workflow with real-time streaming and monitoring...")
        print("   📡 Watch progress and output in real-time:")
        print("   " + "=" * 60)
        
        # Execute with streaming for real-time monitoring
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Streaming monitored workflow completed successfully!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Monitoring Workflow")
            self._maybe_show_full_output(result, "Monitoring Workflow Results")
        else:
            print("   ❌ Streaming monitored workflow failed")
            
        # Method 2: Traditional asynchronous monitoring (for comparison)
        print("\n🛠️  Method 2: Traditional Asynchronous Monitoring")
        
        workflow = self.client.create_workflow(
            "async_monitored_workflow",
            "Traditional Async Workflow with Polling"
        )
        
        # Add steps for traditional monitoring demo
        workflow.add_step("step1", "research_assistant", "Research quantum computing applications")
        workflow.add_step("step2", "data_analyst", "Analyze quantum computing market data")
        workflow.add_step("step3", "content_creator", "Create market analysis report")
        workflow.add_step("step4", "qa_specialist", "Review and validate the analysis")
        
        print("   ✓ Research step added")
        print("   ✓ Analysis step added") 
        print("   ✓ Content creation step added")
        print("   ✓ Review step added")
        
        print("\n▶️  Executing workflow with traditional async monitoring...")
        
        # Execute with monitoring (simulated async for demo)
        execution_id = self.client.start_workflow_execution(workflow)
        if execution_id:
            print(f"   📋 Execution ID: {execution_id}")
            
            # Monitor progress with polling
            print("   🔄 Monitoring progress with polling...")
            while True:
                status = self.client.get_workflow_status(execution_id)
                if not status:
                    break
                    
                print(f"   📊 Status: {status.get('status', 'unknown')}")
                print(f"   🔄 Progress: {status.get('completed_steps', 0)}/{status.get('total_steps', 0)} steps")
                
                if status.get('status') in ['completed', 'failed', 'cancelled']:
                    break
                    
                time.sleep(2)
                
            # Get final results
            result = self.client.get_workflow_result(execution_id)
            if result:
                print("   ✅ Async monitored workflow completed successfully!")
                self._show_result_summary(result)
                self._ensure_output_displayed(result, "Async Monitoring Workflow")
            else:
                print("   ❌ Async monitored workflow failed")
        else:
            print("   ❌ Failed to start workflow execution")
            
        # Method 3: Comparison demo
        print("\n📊 Method 3: Streaming vs Traditional Comparison")
        print("   🌊 Streaming Benefits:")
        print("     • Real-time output visibility")
        print("     • Immediate feedback on progress")
        print("     • Lower latency for user experience")
        print("     • Better error detection and debugging")
        print("     • More engaging user interaction")
        
        print("   🛠️  Traditional Benefits:")
        print("     • Better for background processing")
        print("     • More suitable for batch operations")
        print("     • Easier resource management")
        print("     • Better for long-running workflows")
        print("     • Simpler error recovery patterns")
            
    def demo_streaming_showcase(self):
        """Dedicated demo to showcase streaming capabilities"""
        print("\n" + "="*60)
        print("🌊 DEMO 6: Streaming Capabilities Showcase")
        print("="*60)
        
        print("\n📡 This demo showcases the power of streaming workflows")
        
        # Demo 1: Simple streaming workflow
        print("\n🚀 Simple Streaming Workflow")
        
        workflow = self.client.create_workflow(
            "simple_streaming_demo",
            "Simple Streaming Demo"
        )
        
        workflow.add_step(
            "generate_content",
            "content_creator",
            "Write a short story about a robot learning to paint",
            temperature=0.8,
            max_tokens=800
        )
        
        print("   ✓ Creative writing step added")
        print("\n▶️  Executing with streaming - watch the story unfold:")
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Simple streaming demo completed!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Simple Streaming Demo")
            self._maybe_show_full_output(result, "Creative Story Output", auto_show_if_short=True)
        else:
            print("   ❌ Simple streaming demo failed")
            
        # Demo 2: Multi-step streaming workflow
        print("\n🔗 Multi-Step Streaming Workflow")
        
        workflow = self.client.create_workflow(
            "multi_step_streaming_demo",
            "Multi-Step Streaming Demo"
        )
        
        workflow.add_step(
            "brainstorm",
            "content_creator",
            "Brainstorm ideas for a sci-fi short story",
            temperature=0.9,
            max_tokens=300
        )
        
        workflow.add_step(
            "outline",
            "content_creator",
            "Create a detailed outline based on the brainstormed ideas",
            temperature=0.6,
            max_tokens=500
        )
        
        workflow.add_step(
            "write",
            "content_creator",
            "Write the complete short story based on the outline",
            temperature=0.7,
            max_tokens=1000
        )
        
        workflow.add_step(
            "edit",
            "qa_specialist",
            "Edit and polish the story for grammar, flow, and style",
            temperature=0.3,
            max_tokens=1000
        )
        
        print("   ✓ Brainstorming step added")
        print("   ✓ Outlining step added")
        print("   ✓ Writing step added")
        print("   ✓ Editing step added")
        
        print("\n▶️  Executing multi-step streaming workflow:")
        print("   📡 Watch each step process in real-time:")
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
        
        if result:
            print("   ✅ Multi-step streaming demo completed!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Multi-Step Streaming Demo")
            self._maybe_show_full_output(result, "Complete Story Development Output")
        else:
            print("   ❌ Multi-step streaming demo failed")
            
        # Demo 3: Streaming vs Non-streaming comparison
        print("\n⚖️  Direct Streaming vs Non-Streaming Comparison")
        
        print("   Creating identical workflows...")
        
        # Create two identical workflows
        streaming_workflow = self.client.create_workflow(
            "comparison_streaming",
            "Comparison Streaming Workflow"
        )
        
        non_streaming_workflow = self.client.create_workflow(
            "comparison_non_streaming", 
            "Comparison Non-Streaming Workflow"
        )
        
        # Add identical steps to both
        prompt = "Analyze the impact of artificial intelligence on modern education systems"
        
        for workflow in [streaming_workflow, non_streaming_workflow]:
            workflow.add_step("analyze", "research_assistant", prompt, max_tokens=600)
        
        print("   ✓ Identical workflows created")
        
        # Execute streaming version
        print("\n   🌊 Executing STREAMING version:")
        print("   " + "-" * 50)
        
        streaming_start = time.time()
        streaming_result = self.client.execute_workflow(streaming_workflow, streaming=True)
        streaming_time = time.time() - streaming_start
        
        print("   " + "-" * 50)
        print(f"   ✅ Streaming completed in {streaming_time:.2f}s")
        
        # Execute non-streaming version
        print("\n   🛠️  Executing NON-STREAMING version:")
        print("   (No real-time output - waiting for completion...)")
        
        non_streaming_start = time.time()
        non_streaming_result = self.client.execute_workflow(non_streaming_workflow, streaming=False)
        non_streaming_time = time.time() - non_streaming_start
        
        print(f"   ✅ Non-streaming completed in {non_streaming_time:.2f}s")
        
        # Show comparison results  
        print("\n   📊 Detailed Comparison Results:")
        print(f"     🌊 Streaming: {streaming_time:.2f}s (Real-time LLM output)")
        print(f"     🛠️  Non-streaming: {non_streaming_time:.2f}s (Batch output)")
        
        user_experience_improvement = "significantly better" if streaming_time <= non_streaming_time else "better"
        print(f"     🎯 User Experience: Streaming provides {user_experience_improvement} feedback")
        
        # Show outputs from both for comparison
        print("\n   � Output Comparison:")
        if streaming_result:
            print("     🌊 Streaming Result Quality: ✅ Available")
            self._show_result_summary(streaming_result)
            self._ensure_output_displayed(streaming_result, "Streaming Comparison")
        else:
            print("     🌊 Streaming Result Quality: ❌ Failed")
            
        if non_streaming_result:
            print("\n     🛠️  Non-Streaming Result Quality: ✅ Available")
            self._show_result_summary(non_streaming_result)
            self._ensure_output_displayed(non_streaming_result, "Non-Streaming Comparison")
        else:
            print("\n     🛠️  Non-Streaming Result Quality: ❌ Failed")
        
        # Show the key benefits that were demonstrated
        self._show_streaming_benefits()
        
    def demo_simple_test(self):
        """Simple test to debug workflow issues"""
        print("\n" + "="*60)
        print("🧪 SIMPLE TEST: Basic Workflow Debugging")
        print("="*60)
        
        print("\n🔍 Testing basic workflow creation and execution")
        
        # Test 1: Simple single-step workflow
        print("\n1️⃣ Testing single-step workflow:")
        
        workflow = self.client.create_workflow(
            "simple_test",
            "Simple Test Workflow"
        )
        
        workflow.add_step(
            "test_step",
            "research_assistant",
            "Write a short explanation of what artificial intelligence is",
            max_tokens=200
        )
        
        self._debug_workflow_creation(workflow)
        
        print("\n▶️  Executing simple test workflow...")
        try:
            result = self.client.execute_workflow(workflow, streaming=False)
            if result:
                print("   ✅ Simple test workflow completed!")
                self._show_result_summary(result)
                self._ensure_output_displayed(result, "Simple Test Workflow")
            else:
                print("   ❌ Simple test workflow failed - no result")
        except Exception as e:
            print(f"   ❌ Simple test workflow failed: {e}")
            logger.error(f"Simple test error: {e}")
            
        # Test 2: Test with streaming
        print("\n2️⃣ Testing same workflow with streaming:")
        
        try:
            print("   📡 Streaming execution with real-time output:")
            result = self._execute_workflow_with_enhanced_output(workflow, streaming=True)
            if result:
                print("   ✅ Streaming test workflow completed!")
                self._show_result_summary(result)
                self._ensure_output_displayed(result, "Streaming Test Workflow")
            else:
                print("   ❌ Streaming test workflow failed - no result")
        except Exception as e:
            print(f"   ❌ Streaming test workflow failed: {e}")
            logger.error(f"Streaming test error: {e}")
            
        # Test 3: Test available agents directly
        print("\n3️⃣ Testing available agents:")
        print(f"   Available agents: {self.available_agents}")
        
        for agent in self.available_agents[:3]:  # Test first 3 agents
            print(f"\n   Testing agent: {agent}")
            test_workflow = self.client.create_workflow(
                f"test_{agent}",
                f"Test {agent}"
            )
            
            test_workflow.add_step(
                "test",
                agent,
                f"Say hello and introduce yourself as {agent}",
                max_tokens=100
            )
            
            try:
                result = self.client.execute_workflow(test_workflow, streaming=False)
                if result:
                    print(f"     ✅ {agent} test passed")
                    # Show what the agent said
                    if 'final_output' in result and result['final_output']:
                        output = str(result['final_output'])
                        # Show first 150 characters of agent response
                        preview = output[:150] + "..." if len(output) > 150 else output
                        print(f"       💬 Agent response: {preview}")
                else:
                    print(f"     ❌ {agent} test failed - no result")
            except Exception as e:
                print(f"     ❌ {agent} test failed: {e}")
                logger.error(f"Agent {agent} test error: {e}")
                
        print("\n🎯 Test Summary:")
        print("   If all tests pass, the issue might be with specific workflow configurations.")
        print("   If tests fail, there might be a server or client connection issue.")
            
    def _show_result_summary(self, result: Dict[str, Any]):
        """Show a comprehensive summary of workflow results with enhanced LLM output display"""
        if not result:
            print("   📭 No results to display")
            return
            
        print("   📋 Comprehensive Result Summary:")
        print("   " + "═" * 60)
        
        # Show execution metadata
        if 'execution_time' in result:
            print(f"   ⏱️  Execution time: {result['execution_time']:.2f}s")
            
        if 'status' in result:
            status_icon = "✅" if result['status'] == 'completed' else "❌"
            print(f"   📊 Status: {status_icon} {result['status']}")
            
        # Show step results with enhanced output display
        step_results = result.get('step_results', {})
        if step_results:
            successful_steps = sum(1 for step in step_results.values() if step.get('success'))
            total_steps = len(step_results)
            print(f"   ✅ Steps completed: {successful_steps}/{total_steps}")
            
            print(f"\n   📄 Step-by-Step LLM Outputs:")
            print("   " + "─" * 60)
            
            for i, (step_name, step_data) in enumerate(step_results.items(), 1):
                success_icon = "✅" if step_data.get('success') else "❌"
                print(f"\n   {i}. {success_icon} {step_name}")
                
                if step_data.get('success') and step_data.get('output'):
                    output = str(step_data['output'])
                    self._display_formatted_output(output, f"Step {i} Output", max_lines=8)
                elif not step_data.get('success') and step_data.get('error'):
                    print(f"      ❌ Error: {step_data.get('error')}")
                else:
                    print(f"      ⚪ No output available")
            
            # Show any failures with details
            failed_steps = [name for name, step in step_results.items() if not step.get('success')]
            if failed_steps:
                print(f"\n   ❌ Failed steps: {', '.join(failed_steps)}")
                
        # Show final output with enhanced formatting
        if 'final_output' in result and result['final_output']:
            print(f"\n   🎯 Final LLM Output:")
            print("   " + "═" * 60)
            output = str(result['final_output'])
            self._display_formatted_output(output, "Final Output", max_lines=15)
        
        # Show workflow context if available
        if 'workflow_context' in result:
            context = result['workflow_context']
            if context:
                print(f"\n   🏷️  Workflow Context:")
                for key, value in context.items():
                    if isinstance(value, str) and len(value) > 100:
                        value = value[:100] + "..."
                    print(f"     • {key}: {value}")
        
        print("   " + "═" * 60)
        print(f"   💡 To see full output, use _show_full_output() method")
            
    def _display_formatted_output(self, output: str, title: str, max_lines: int = 10):
        """Display LLM output with proper formatting and line numbering"""
        if not output or not output.strip():
            print(f"      📭 {title}: (empty)")
            return
            
        lines = output.strip().split('\n')
        total_lines = len(lines)
        
        # Show line-by-line output with numbers
        for i, line in enumerate(lines[:max_lines], 1):
            # Clean up the line for better display
            clean_line = line.rstrip()
            if len(clean_line) > 120:
                clean_line = clean_line[:117] + "..."
            print(f"   {i:3d} │ {clean_line}")
        
        # Show truncation info if needed
        if total_lines > max_lines:
            remaining = total_lines - max_lines
            print(f"   ... │ ({remaining} more lines - use _show_full_output() to see all)")
            
        # Show character/word count
        char_count = len(output)
        word_count = len(output.split())
        print(f"      ℹ️  {total_lines} lines, {word_count} words, {char_count} characters")

    def _ensure_output_displayed(self, result: Dict[str, Any], context: str = "Workflow"):
        """Ensure LLM output is always displayed to the user, regardless of streaming success"""
        if not result:
            print(f"   📭 No {context.lower()} output to display")
            return

        print(f"\n   🎯 {context} LLM Output:")
        print("   " + "═" * 60)

        # Show final output if available
        if 'final_output' in result and result['final_output']:
            output = str(result['final_output'])
            lines = output.split('\n')
            
            # Show the output with line numbers for easy reference
            for i, line in enumerate(lines, 1):
                print(f"   {i:3d} │ {line}")
            
            # Show output statistics
            char_count = len(output)
            word_count = len(output.split())
            line_count = len(lines)
            print(f"\n   📊 Output Stats: {line_count} lines, {word_count} words, {char_count} characters")
            
        elif 'step_results' in result:
            # If no final output, show the last successful step output
            step_results = result['step_results']
            found_output = False
            for step_name, step_data in reversed(list(step_results.items())):
                if step_data.get('success'):
                    # Check multiple possible locations for step output
                    output = None
                    if step_data.get('output'):
                        output = str(step_data['output'])
                    elif step_data.get('result_data', {}).get('text'):
                        output = str(step_data['result_data']['text'])
                    elif step_data.get('result'):
                        output = str(step_data['result'])
                    
                    if output:
                        print(f"   📝 Output from step '{step_name}':")
                        lines = output.split('\n')
                        for i, line in enumerate(lines[:15], 1):  # Show first 15 lines
                            print(f"   {i:3d} │ {line}")
                        if len(lines) > 15:
                            print(f"   ... │ ({len(lines) - 15} more lines)")
                        
                        # Show output statistics
                        char_count = len(output)
                        word_count = len(output.split())
                        line_count = len(lines)
                        print(f"\n   📊 Output Stats: {line_count} lines, {word_count} words, {char_count} characters")
                        found_output = True
                        break
            
            if not found_output:
                print(f"   📭 No successful step outputs found in {context.lower()}")
        else:
            print(f"   📭 No output found in {context.lower()} result")
        
        print("   " + "═" * 60)
        
        # Always print the full response JSON after the LLM output
        self._print_full_response_json(result, context)

    def _print_full_response_json(self, result: Dict[str, Any], context: str = "Workflow"):
        """Always print the complete response JSON for full transparency"""
        if not result:
            print(f"   📭 No {context.lower()} response data to display")
            return

        print(f"\n   📋 Complete {context} Response JSON:")
        print("   " + "═" * 80)
        
        try:
            # Pretty-print the JSON with proper indentation
            json_str = json.dumps(result, indent=2, ensure_ascii=False)
            lines = json_str.split('\n')
            
            # Add line numbers and indentation for readability
            for i, line in enumerate(lines, 1):
                # Truncate very long lines to keep display manageable
                if len(line) > 120:
                    display_line = line[:117] + "..."
                else:
                    display_line = line
                print(f"   {i:3d} │ {display_line}")
            
            # Show JSON statistics
            total_chars = len(json_str)
            total_lines = len(lines)
            print(f"\n   📊 JSON Stats: {total_lines} lines, {total_chars} characters")
            
        except (TypeError, ValueError) as e:
            # Fallback to repr if JSON serialization fails
            print(f"   ⚠️  JSON serialization failed: {e}")
            print(f"   📄 Raw response data:")
            result_str = str(result)
            lines = result_str.split('\n')
            for i, line in enumerate(lines[:50], 1):  # Show first 50 lines
                if len(line) > 120:
                    display_line = line[:117] + "..."
                else:
                    display_line = line
                print(f"   {i:3d} │ {display_line}")
            if len(lines) > 50:
                print(f"   ... │ ({len(lines) - 50} more lines truncated)")
        
        print("   " + "═" * 80)

    def _maybe_show_full_output(self, result: Dict[str, Any], title: str = "Complete Output", auto_show_if_short: bool = True):
        """Conditionally show full output based on result size and user preference"""
        if not result:
            return
            
        # Check if output is short enough to show automatically
        final_output = result.get('final_output', '')
        if auto_show_if_short and final_output:
            output_length = len(str(final_output))
            if output_length < 2000:  # Show automatically if less than 2000 characters
                self._show_full_output(result, title)
                return
        
        # For longer outputs, just mention it's available
        print(f"\n   💡 Full output available - call _show_full_output() to see complete {title.lower()}")

    def _show_streaming_benefits(self):
        """Show the key benefits that streaming provides"""
        print("\n   🌟 Key Streaming Benefits Demonstrated:")
        print("   " + "─" * 60)
        print("     🚀 Real-time feedback - See AI working in real-time")
        print("     ⚡ Better user experience - No waiting for batch completion")
        print("     🐛 Early error detection - Spot issues as they happen")
        print("     📊 Progress visibility - Know which step is currently running")
        print("     🎯 Token-by-token output - Watch content being generated")
        print("     🔄 Graceful fallbacks - Automatic retry with non-streaming")
        print("   " + "─" * 60)

    def run_interactive_mode(self):
        """Run interactive mode for custom workflow creation"""
        print("\n" + "="*60)
        print("🎮 INTERACTIVE MODE")
        print("="*60)
        
        print("\n🎯 Create your own custom workflow!")
        print("Available agents:", ", ".join(self.available_agents))
        
        workflow_name = input("\nEnter workflow name: ").strip()
        if not workflow_name:
            workflow_name = "interactive_workflow"
            
        workflow = self.client.create_workflow(workflow_name, f"Interactive {workflow_name}")
        
        step_count = 0
        while True:
            step_count += 1
            print(f"\n📝 Step {step_count}:")
            
            step_id = input("  Step ID (or 'done' to finish): ").strip()
            if step_id.lower() == 'done':
                break
                
            agent_name = input("  Agent name: ").strip()
            if agent_name not in self.available_agents:
                agent_name = self._validate_agent_name(agent_name)
                
            task = input("  Task description: ").strip()
            if not task:
                print("  ⚠️  Task description required!")
                continue
                
            workflow.add_step(step_id, agent_name, task)
            print(f"  ✅ Step {step_count} added!")
            
        if step_count == 1:
            print("\n⚠️  No steps added to workflow")
            return
            
        print(f"\n🚀 Executing your {step_count-1}-step workflow...")
        
        use_streaming = input("Use streaming? (y/n): ").strip().lower() == 'y'
        
        result = self._execute_workflow_with_enhanced_output(workflow, streaming=use_streaming)
        
        if result:
            print("\n✅ Interactive workflow completed!")
            self._show_result_summary(result)
            self._ensure_output_displayed(result, "Interactive Workflow")
            
            show_full = input("\nShow full output? (y/n): ").strip().lower() == 'y'
            if show_full:
                self._show_full_output(result, "Interactive Workflow Results")
        else:
            print("\n❌ Interactive workflow failed")

    def run_all_demos(self):
        """Run all demonstration workflows"""
        print("\n🎪 RUNNING ALL DEMOS")
        print("="*60)
        
        demos = [
            ("Content Creation", self.demo_content_creation_workflow),
            ("Code Development", self.demo_code_development_workflow),
            ("Data Analysis", self.demo_data_analysis_workflow),
            ("Collaborative Workflow", self.demo_collaborative_workflow),
            ("Workflow Monitoring", self.demo_workflow_monitoring),
            ("Streaming Showcase", self.demo_streaming_showcase),
            ("Simple Test", self.demo_simple_test),
        ]
        
        for demo_name, demo_func in demos:
            try:
                print(f"\n🎯 Starting {demo_name} Demo...")
                demo_func()
                print(f"✅ {demo_name} Demo completed!")
            except KeyboardInterrupt:
                print(f"\n⏸️  {demo_name} Demo interrupted by user")
                break
            except Exception as e:
                print(f"❌ {demo_name} Demo failed: {e}")
                logger.error(f"{demo_name} demo error: {e}")
                continue
                
        print("\n🏁 All demos completed!")

def main():
    """Main function with command-line argument handling"""
    parser = argparse.ArgumentParser(
        description="Updated Sequential Workflow Demo for Kolosal Server",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python sequential_workflow_demo_updated.py                    # Run all demos
  python sequential_workflow_demo_updated.py --demo streaming   # Show streaming demo only
  python sequential_workflow_demo_updated.py --interactive      # Interactive mode
  python sequential_workflow_demo_updated.py --host 192.168.1.100 --port 9090
        """
    )
    
    parser.add_argument("--host", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--demo", choices=["streaming", "content", "code", "data", "collaborative", "monitoring", "test"], 
                       help="Run specific demo only")
    parser.add_argument("--interactive", action="store_true", help="Run in interactive mode")
    parser.add_argument("--debug", action="store_true", help="Enable debug mode with detailed output")
    
    args = parser.parse_args()
    
    try:
        # Initialize the demo
        demo = UpdatedSequentialWorkflowDemo(host=args.host, port=args.port, debug=args.debug)
        
        if args.interactive:
            # Run interactive mode
            demo.run_interactive_mode()
        elif args.demo:
            # Run specific demo
            demo_map = {
                "streaming": demo.demo_streaming_showcase,
                "content": demo.demo_content_creation_workflow,
                "code": demo.demo_code_development_workflow,
                "data": demo.demo_data_analysis_workflow,
                "collaborative": demo.demo_collaborative_workflow,
                "monitoring": demo.demo_workflow_monitoring,
                "test": demo.demo_simple_test
            }
            
            if args.demo in demo_map:
                print(f"\n🎯 Running {args.demo.title()} Demo")
                demo_map[args.demo]()
            else:
                print(f"❌ Unknown demo: {args.demo}")
                return 1
        else:
            # Run all demos
            demo.run_all_demos()
            
        print("\n🎉 Demo completed successfully!")
        return 0
        
    except KeyboardInterrupt:
        print("\n⏸️  Demo interrupted by user")
        return 0
    except Exception as e:
        print(f"\n❌ Demo failed: {e}")
        logger.error(f"Main demo error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())

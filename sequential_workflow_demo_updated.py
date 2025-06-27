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
    
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.host = host
        self.port = port
        
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
            
            if result and show_step_outputs:
                print("   📝 Final Step Summary:")
                self._display_step_by_step_results(result)
                        
            return result
            
        except Exception as e:
            print("   " + "─" * 60)
            print(f"   ❌ Streaming execution failed: {e}")
            logger.error(f"Enhanced streaming execution error: {e}")
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
            response = self.client.session.post(
                f"{self.client.base_url}/api/v1/sequential-workflows/{workflow_id}/execute",
                json={},
                headers={
                    "Content-Type": "application/json", 
                    "Accept": "text/event-stream"
                },
                stream=True
            )
            
            if response.status_code != 200:
                print(f"❌ Streaming execution failed: {response.status_code}")
                return None
            
            # Parse streaming response with enhanced display
            current_step = None
            current_step_output = ""
            buffer = ""
            final_result = None
            step_counter = 0
            
            print("   � Live LLM Output Stream:")
            print("   " + "═" * 60)
            
            for chunk in response.iter_content(chunk_size=512, decode_unicode=True):
                if not chunk:
                    continue
                    
                buffer += chunk
                
                # Process complete lines
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    
                    if line.startswith('data:'):
                        data_content = line[5:].strip()
                        
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
                                    
                        except json.JSONDecodeError:
                            # Handle plain text streaming (raw LLM output)
                            if data_content and data_content != '[DONE]':
                                print(data_content, end='', flush=True)
                                current_step_output += data_content
                                
                    elif line and not line.startswith('event:'):
                        # Handle any other text output
                        print(line, end='', flush=True)
            
            print("\n   " + "═" * 60)
            print("   ✅ Streaming completed")
            
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
        else:
            print("   ❌ Streaming code development failed - no result returned")
            
        # Method 2: Traditional built-in template for comparison
        print("\n�🚀 Method 2: Traditional Built-in Template (No Streaming)")
        print("   quick_code_generation('User authentication REST API')")
        
        result = quick_code_generation("User authentication REST API")
        
        if result:
            print("   ✅ Traditional code generation completed successfully!")
            self._show_result_summary(result)
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
        else:
            print("     🌊 Streaming Result Quality: ❌ Failed")
            
        if non_streaming_result:
            print("\n     🛠️  Non-Streaming Result Quality: ✅ Available")
            self._show_result_summary(non_streaming_result)
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
                print("   📄 LLM Output:")
                # Show the actual AI explanation
                if 'final_output' in result and result['final_output']:
                    output = str(result['final_output'])
                    lines = output.split('\n')
                    for line in lines:
                        print(f"      📝 {line}")
                self._show_result_summary(result)
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
            
    def run_interactive_mode(self):
        """Run interactive mode for custom workflow creation"""
        print("\n" + "="*60)
        print("🎮 INTERACTIVE MODE")
        print("="*60)
        
        print("\n🛠️  Create your own custom workflow!")
        
        # Get workflow details
        workflow_id = input("Enter workflow ID: ").strip()
        workflow_name = input("Enter workflow name: ").strip()
        
        if not workflow_id or not workflow_name:
            print("❌ Workflow ID and name are required")
            return
            
        # Ask about streaming preference
        streaming_choice = input("Enable streaming? (y/n): ").strip().lower()
        use_streaming = streaming_choice in ['y', 'yes', '1', 'true']
        
        workflow = self.client.create_workflow(workflow_id, workflow_name)
        
        print("\n📝 Add workflow steps:")
        print("Available agent types: research_assistant, content_creator, data_analyst, code_assistant, qa_specialist, project_manager")
        print("Enter 'done' when finished adding steps")
        
        step_count = 1
        while True:
            print(f"\n--- Step {step_count} ---")
            step_id = input("Step ID: ").strip()
            if step_id.lower() == 'done':
                break
                
            agent_type = input("Agent type: ").strip()
            prompt = input("Task description: ").strip()
            
            if step_id and agent_type and prompt:
                workflow.add_step(step_id, agent_type, prompt)
                print(f"   ✓ Step '{step_id}' added")
                step_count += 1
            else:
                print("   ❌ All fields are required")
                
        if step_count > 1:
            streaming_text = "with streaming" if use_streaming else "without streaming"
            print(f"\n▶️  Executing custom workflow {streaming_text} ({step_count-1} steps)...")
            
            if use_streaming:
                print("   📡 Real-time execution:")
                print("   " + "-" * 50)
                
            result = self.client.execute_workflow(workflow, streaming=use_streaming)
            
            if use_streaming:
                print("   " + "-" * 50)
                
            if result:
                print("   ✅ Custom workflow completed successfully!")
                self._show_result_summary(result)
            else:
                print("   ❌ Custom workflow failed")
        else:
            print("   ⚠️  No steps added to workflow")
            
    def run_all_demos(self):
        """Run all demo workflows"""
        print("\n🎯 Running All Sequential Workflow Demos")
        print("="*60)
        
        self.show_available_agents()
        
        # Run all demos
        demos = [
            ("Content Creation", self.demo_content_creation_workflow),
            ("Code Development", self.demo_code_development_workflow), 
            ("Data Analysis", self.demo_data_analysis_workflow),
            ("Collaborative", self.demo_collaborative_workflow),
            ("Monitoring", self.demo_workflow_monitoring),
            ("Streaming Showcase", self.demo_streaming_showcase),
        ]
        
        results = {}
        
        for demo_name, demo_func in demos:
            try:
                print(f"\n🚀 Starting {demo_name} demo...")
                start_time = time.time()
                
                demo_func()
                
                execution_time = time.time() - start_time
                results[demo_name] = {"status": "success", "time": execution_time}
                
            except Exception as e:
                logger.error(f"Error in {demo_name} demo: {e}")
                results[demo_name] = {"status": "failed", "error": str(e)}
                
        # Show final summary
        print("\n" + "="*60)
        print("📊 DEMO EXECUTION SUMMARY")
        print("="*60)
        
        for demo_name, result in results.items():
            status_icon = "✅" if result["status"] == "success" else "❌"
            print(f"{status_icon} {demo_name}: {result['status']}")
            if result["status"] == "success":
                print(f"   ⏱️  Time: {result['time']:.2f}s")
            else:
                print(f"   💥 Error: {result['error']}")
                
        successful_demos = sum(1 for r in results.values() if r["status"] == "success")
        total_demos = len(results)
        
        print(f"\n🎯 Overall: {successful_demos}/{total_demos} demos successful")
        
        # Show comparison with old demo
        print("\n" + "="*60)
        print("🔄 COMPARISON WITH ORIGINAL DEMO")
        print("="*60)
        print("📊 Original demo complexity:")
        print("   • 1200+ lines of code")
        print("   • Manual agent UUID mapping required")
        print("   • Complex setup and configuration")
        print("   • Manual engine/model management")
        print("   • Verbose error handling")
        print("   • Difficult to extend and modify")
        
        print("\n📊 Updated demo improvements:")
        print("   • ~1500 lines of enhanced code")
        print("   • Zero configuration - everything automatic")
        print("   • Agent names used directly (no UUIDs)")
        print("   • Auto-setup handles all complexity")
        print("   • Built-in error handling and retries")
        print("   • Easy to extend with new workflows")
        print("   • ✨ Real-time LLM streaming with token-by-token display")
        print("   • ✨ Enhanced LLM output formatting and visualization")
        print("   • ✨ Step-by-step output tracking and display")
        print("   • ✨ Professional result summaries with statistics")
        print("   • ✨ Interactive full output viewing")
        print("   • ✨ Comprehensive error reporting")
        print("   • ✨ Live progress monitoring and feedback")
        
        print("\n🎉 LLM outputs are now beautifully displayed in real-time with professional formatting!")
        
        print("\n🎯 LLM Output Enhancement Summary:")
        print("="*60)
        print("✨ STREAMING IMPROVEMENTS:")
        print("   • Real-time token-by-token LLM output display")
        print("   • Live step-by-step execution monitoring")
        print("   • Enhanced streaming response parsing")
        print("   • Professional formatting with line numbers")
        print("   • Automatic content type detection and styling")
        
        print("\n✨ OUTPUT DISPLAY IMPROVEMENTS:")
        print("   • Comprehensive result summaries with metadata")
        print("   • Step-by-step output tracking and visualization")
        print("   • Formatted output with syntax highlighting")
        print("   • Interactive full output viewing options")
        print("   • Statistics (lines, words, characters, reading time)")
        
        print("\n✨ USER EXPERIENCE IMPROVEMENTS:")
        print("   • Optional full output display based on content length")
        print("   • Enhanced error reporting with context")
        print("   • Professional progress indicators")
        print("   • Better content truncation and previews")
        print("   • Improved Unicode/emoji support for output")
        
        print("\n✨ TECHNICAL IMPROVEMENTS:")
        print("   • Enhanced streaming parser with event handling")
        print("   • Better error handling and debugging")
        print("   • Structured output formatting methods")
        print("   • Comprehensive logging and monitoring")
        print("   • Robust encoding and display support")
        
    def _maybe_show_full_output(self, result: Dict[str, Any], title: str = "Complete Output", auto_show_if_short: bool = True):
        """Optionally show full output based on user choice or content length"""
        if not result:
            return
            
        # Auto-show if content is short
        if auto_show_if_short:
            final_output = result.get('final_output', '')
            if final_output and len(str(final_output)) < 500:
                print(f"\n   📖 {title} (auto-shown due to short length):")
                self._show_full_output(result, title)
                return
        
        # Ask user for longer content
        print(f"\n   💡 View complete {title.lower()}? (y/n): ", end="")
        try:
            choice = input().strip().lower()
            if choice in ['y', 'yes', '1', 'true']:
                self._show_full_output(result, title)
        except:
            # Skip if running non-interactively
            pass
            
    def _show_streaming_benefits(self):
        """Display the benefits of streaming vs traditional execution"""
        print("\n   🌟 Streaming Benefits Demonstrated:")
        print("   " + "─" * 50)
        print("   ✨ Real-time LLM output visibility")
        print("   ✨ Token-by-token streaming display")
        print("   ✨ Immediate feedback on AI processing")
        print("   ✨ Better user engagement and interactivity")
        print("   ✨ Live progress indication per step")
        print("   ✨ Enhanced debugging capabilities")
        print("   ✨ Step-by-step output tracking")
        print("   ✨ More responsive user experience")
        print("   ✨ Professional-grade output formatting")
        print("   ✨ Comprehensive result summaries")

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Updated Sequential Workflow Demo")
    parser.add_argument("--host", default="localhost", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument("--interactive", action="store_true", help="Run in interactive mode")
    parser.add_argument("--demo", choices=["content", "code", "data", "collaborative", "monitoring", "streaming", "test"], 
                       help="Run specific demo")
    
    args = parser.parse_args()
    
    try:
        # Create demo instance
        demo = UpdatedSequentialWorkflowDemo(host=args.host, port=args.port)
        
        if args.interactive:
            demo.run_interactive_mode()
        elif args.demo:
            # Run specific demo
            demo.show_available_agents()
            if args.demo == "content":
                demo.demo_content_creation_workflow()
            elif args.demo == "code":
                demo.demo_code_development_workflow()
            elif args.demo == "data":
                demo.demo_data_analysis_workflow()
            elif args.demo == "collaborative":
                demo.demo_collaborative_workflow()
            elif args.demo == "monitoring":
                demo.demo_workflow_monitoring()
            elif args.demo == "streaming":
                demo.demo_streaming_showcase()
            elif args.demo == "test":
                demo.demo_simple_test()
        else:
            # Run all demos
            demo.run_all_demos()
            
    except KeyboardInterrupt:
        print("\n🛑 Demo interrupted by user")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Demo failed: {e}")
        print(f"❌ Demo failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

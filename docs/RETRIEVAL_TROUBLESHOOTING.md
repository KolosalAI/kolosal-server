# Document Retrieval Troubleshooting Guide

This guide helps diagnose and fix common issues with the document retrieval system.

## Quick Diagnostics

### 1. Health Check Endpoint
```bash
curl http://localhost:8080/health
```
This endpoint now includes detailed document service health information including:
- Service initialization status
- Qdrant database connection status
- Collection existence and configuration
- Recommendations for fixing issues

### 2. Retrieval Test Endpoint
```bash
curl http://localhost:8080/retrieve/test
```
This endpoint runs comprehensive tests including:
- Document service availability
- Database connection test
- Embedding generation test
- Simple retrieval test
- Detailed recommendations for failures

## Common Issues and Solutions

### Issue: "No documents found"

**Possible Causes:**
1. **Empty Collection**: No documents have been indexed yet
2. **Collection Doesn't Exist**: The 'documents' collection hasn't been created
3. **High Score Threshold**: The similarity threshold is too restrictive
4. **Query Mismatch**: The query doesn't match indexed content

**Solutions:**
- Check collection status: `GET /health`
- Index some documents first: `POST /api/v1/documents`
- Lower the `score_threshold` parameter (try 0.0)
- Try broader, simpler queries

### Issue: "Database connection failed"

**Possible Causes:**
1. Qdrant server not running
2. Wrong connection configuration
3. Network connectivity issues

**Solutions:**
- Start Qdrant: `docker run -p 6333:6333 qdrant/qdrant`
- Check configuration in `config.yaml`
- Verify Qdrant is accessible: `curl http://localhost:6333/`

### Issue: "Embedding generation failed"

**Possible Causes:**
1. Embedding model not loaded
2. Model configuration issues
3. Inference engine problems

**Solutions:**
- Check if embedding model is configured in `config.yaml`
- Verify model files are available
- Check inference engine status: `GET /health`

### Issue: "Service not initialized"

**Possible Causes:**
1. Server starting up
2. Configuration issues
3. Qdrant not accessible during startup

**Solutions:**
- Wait for server to fully start
- Check server logs for initialization errors
- Verify Qdrant is running before starting the server

## Improved Error Messages

The system now provides detailed error messages with specific error types:

- `service_not_ready`: Service is starting up
- `service_disabled`: Qdrant disabled in configuration
- `embedding_error`: Issues with embedding generation
- `search_error`: Vector search problems
- `collection_not_found`: No documents collection exists

Each error message includes suggestions to check the health endpoint for more details.

## Monitoring and Logging

The system now provides enhanced logging for debugging:

- Request validation with detailed parameter information
- Database connection status with specific error messages
- Embedding generation progress and dimension information
- Search result parsing with detailed statistics
- Collection existence checks with helpful warnings

## API Improvements

### Enhanced Request Validation
- Better error messages for invalid parameters
- Query length limits (max 10,000 characters)
- Detailed validation logging

### Improved Response Information
- More detailed error messages with context
- Helpful suggestions in error responses
- References to diagnostic endpoints

### New Diagnostic Endpoints
- `GET /health` - Enhanced with document service status
- `GET /retrieve/test` - Comprehensive retrieval testing

## Example Usage

### Basic Retrieval with Error Handling
```bash
# Test basic retrieval
curl -X POST http://localhost:8080/retrieve \
  -H "Content-Type: application/json" \
  -d '{
    "query": "machine learning",
    "k": 5,
    "score_threshold": 0.0
  }'

# If it fails, check diagnostics
curl http://localhost:8080/health
curl http://localhost:8080/retrieve/test
```

### Troubleshooting Workflow
1. Run retrieval test: `GET /retrieve/test`
2. Check health status: `GET /health`
3. Follow recommendations in the responses
4. Check server logs for detailed error information
5. Retry retrieval after fixing identified issues

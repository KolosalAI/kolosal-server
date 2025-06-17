# Authentication Features

The Kolosal Server includes comprehensive authentication features including API key authentication, rate limiting, and CORS (Cross-Origin Resource Sharing) support.

## Features

### 1. API Key Authentication
- **Header-based authentication**: Supports custom headers (X-API-Key, Authorization, etc.)
- **Bearer token support**: Automatically handles "Bearer " prefix for Authorization headers
- **Multiple API keys**: Support for multiple valid API keys
- **Optional/Required modes**: Can be configured as optional or required
- **Real-time management**: Add/remove API keys without server restart

### 2. Rate Limiting
- **Sliding window algorithm**: Tracks requests over a configurable time window
- **Per-client tracking**: Rate limits are applied per client IP address
- **Configurable limits**: Customize max requests and time window
- **Automatic cleanup**: Removes old request data to prevent memory leaks
- **Statistics**: Get real-time statistics about rate limit usage

### 3. CORS Support
- **Origin validation**: Configure allowed origins (domains)
- **Method validation**: Specify which HTTP methods are allowed
- **Header validation**: Control which headers can be sent
- **Preflight handling**: Automatic handling of OPTIONS preflight requests
- **Credentials support**: Optional support for sending credentials

### 4. Unified Middleware
- **Combined processing**: API key auth, rate limiting, and CORS in one middleware
- **Easy configuration**: Simple API for updating settings
- **Production ready**: Optimized for high-performance scenarios

## Configuration

### API Key Authentication Configuration

```cpp
kolosal::auth::AuthMiddleware::ApiKeyConfig apiKeyConfig;
apiKeyConfig.enabled = true;                          // Enable API key authentication
apiKeyConfig.required = true;                         // Make API key required for all requests
apiKeyConfig.headerName = "X-API-Key";               // Header name for API key
apiKeyConfig.validKeys = {"sk-1234", "sk-5678"};    // Valid API keys
```

**YAML Configuration:**
```yaml
auth:
  enabled: true
  require_api_key: true
  api_key_header: "X-API-Key"
  api_keys:
    - "sk-1234567890abcdef"
    - "sk-fedcba0987654321"
```

### Rate Limiting Configuration

```cpp
kolosal::auth::RateLimiter::Config rateLimitConfig;
rateLimitConfig.maxRequests = 100;                    // Max requests per window
rateLimitConfig.windowSize = std::chrono::seconds(60); // Time window (60 seconds)
rateLimitConfig.enabled = true;                       // Enable rate limiting
```

### CORS Configuration

```cpp
kolosal::auth::CorsHandler::Config corsConfig;
corsConfig.allowedOrigins = {"https://example.com", "https://app.example.com"};
corsConfig.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
corsConfig.allowedHeaders = {"Content-Type", "Authorization", "X-API-Key"};
corsConfig.allowCredentials = true;
corsConfig.maxAge = 3600; // Preflight cache duration in seconds
corsConfig.enabled = true;
```

## API Endpoints

### Get Authentication Configuration
```http
GET /v1/auth/config
```

**Response:**
```json
{
  "rate_limiter": {
    "enabled": true,
    "max_requests": 100,
    "window_seconds": 60
  },
  "cors": {
    "enabled": true,
    "allowed_origins": ["*"],
    "allowed_methods": ["GET", "POST", "PUT", "DELETE", "OPTIONS"],
    "allowed_headers": ["Content-Type", "Authorization"],
    "allow_credentials": false,
    "max_age": 86400
  },
  "api_key": {
    "enabled": true,
    "required": true,
    "header_name": "X-API-Key",
    "keys_count": 2
  }
}
```

### Update Authentication Configuration
```http
PUT /v1/auth/config
Content-Type: application/json

{
  "rate_limiter": {
    "enabled": true,
    "max_requests": 50,
    "window_seconds": 60
  },
  "cors": {
    "allowed_origins": ["https://example.com"],
    "allow_credentials": true
  },
  "api_key": {
    "enabled": true,
    "required": true,
    "header_name": "Authorization",
    "api_keys": ["sk-1234567890abcdef", "sk-fedcba0987654321"]
  }
}
```

### Get Rate Limiting Statistics
```http
POST /v1/auth/stats
Content-Type: application/json

{
  "action": "get_stats"
}
```

**Response:**
```json
{
  "rate_limit_stats": {
    "total_clients": 5,
    "clients": {
      "192.168.1.100": 25,
      "192.168.1.101": 10
    }
  },
  "cors_stats": {
    "total_requests": 1000,
    "blocked_requests": 5,
    "preflight_requests": 50
  }
}
```

### Clear Rate Limit Data
```http
POST /v1/auth/clear
Content-Type: application/json

{
  "action": "clear_rate_limit",
  "client_ip": "192.168.1.100"
}
```

Or clear all data:
```http
POST /v1/auth/clear
Content-Type: application/json

{
  "action": "clear_rate_limit",
  "clear_all": true
}
```

## Response Headers

The server automatically adds the following headers to responses:

### Rate Limiting Headers
- `X-Rate-Limit-Limit`: Maximum requests allowed in the current window
- `X-Rate-Limit-Remaining`: Number of requests remaining in the current window
- `X-Rate-Limit-Reset`: Seconds until the rate limit window resets
- `Retry-After`: (on 429 responses) Seconds to wait before retrying

### CORS Headers
- `Access-Control-Allow-Origin`: Allowed origin for the request
- `Access-Control-Allow-Methods`: Allowed methods for preflight requests
- `Access-Control-Allow-Headers`: Allowed headers for preflight requests
- `Access-Control-Allow-Credentials`: Whether credentials are allowed
- `Access-Control-Max-Age`: How long to cache preflight responses
- `Access-Control-Expose-Headers`: Headers exposed to the browser

## Error Responses

### API Key Authentication Failed (401)
```json
{
  "error": {
    "message": "Invalid or missing API key",
    "type": "authentication_error",
    "code": 401
  }
}
```

### Rate Limit Exceeded (429)
```json
{
  "error": {
    "message": "Rate limit exceeded",
    "type": "rate_limit_exceeded",
    "code": 429
  }
}
```

### CORS Policy Violation (403)
```json
{
  "error": {
    "message": "CORS policy violation",
    "type": "authentication_error",
    "code": 403
  }
}
```

## Usage Examples

### Making Authenticated Requests

**Using X-API-Key header:**
```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "X-API-Key: sk-1234567890abcdef" \
  -d '{"messages": [{"role": "user", "content": "Hello"}]}'
```

**Using Authorization header:**
```bash
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer sk-1234567890abcdef" \
  -d '{"messages": [{"role": "user", "content": "Hello"}]}'
```

**JavaScript/Fetch example:**
```javascript
fetch('http://localhost:8080/v1/chat/completions', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
    'X-API-Key': 'sk-1234567890abcdef'
  },
  body: JSON.stringify({
    messages: [{ role: 'user', content: 'Hello' }]
  })
});
```

## Security Considerations

1. **API Key Security**: Store API keys securely and rotate them regularly
2. **Rate Limiting**: Helps prevent abuse and DoS attacks
3. **CORS**: Prevents unauthorized cross-origin requests
4. **Origin Validation**: Always validate allowed origins in production
5. **Header Control**: Limit allowed headers to reduce attack surface
6. **Credentials**: Only enable credentials when necessary

## Best Practices

1. **API Key Management**: 
   - Use strong, randomly generated API keys
   - Implement key rotation policies
   - Store keys securely (environment variables, secrets management)
   - Don't log API keys in plaintext

2. **Production Configuration**: Use stricter limits in production
3. **Monitor Statistics**: Regularly check rate limit and authentication statistics
4. **Whitelist Origins**: Don't use "*" for origins in production
5. **Regular Cleanup**: The system automatically cleans up old data
6. **Logging**: Monitor logs for blocked requests and authentication failures

## Examples

### Development Setup
```cpp
// Relaxed settings for development
auto authMiddleware = kolosal::auth::createDevelopmentAuthMiddleware();
```

### Production Setup
```cpp
// Strict settings for production
auto authMiddleware = kolosal::auth::createProductionAuthMiddleware();

// Add API keys
authMiddleware->addApiKey("sk-production-key-1");
authMiddleware->addApiKey("sk-production-key-2");

// Customize for specific requirements
auto corsConfig = authMiddleware->getCorsConfig();
corsConfig.allowedOrigins = {"https://yourdomain.com", "https://api.yourdomain.com"};
corsConfig.allowCredentials = true;
authMiddleware->updateCorsConfig(corsConfig);

auto rateLimitConfig = authMiddleware->getRateLimiterConfig();
rateLimitConfig.maxRequests = 60; // 1 request per second average
authMiddleware->updateRateLimiterConfig(rateLimitConfig);

// Configure API key authentication
auto apiKeyConfig = authMiddleware->getApiKeyConfig();
apiKeyConfig.enabled = true;
apiKeyConfig.required = true;
apiKeyConfig.headerName = "Authorization";
authMiddleware->updateApiKeyConfig(apiKeyConfig);
```

# Authentication Features

The Kolosal Server now includes comprehensive authentication features including rate limiting and CORS (Cross-Origin Resource Sharing) support.

## Features

### 1. Rate Limiting
- **Sliding window algorithm**: Tracks requests over a configurable time window
- **Per-client tracking**: Rate limits are applied per client IP address
- **Configurable limits**: Customize max requests and time window
- **Automatic cleanup**: Removes old request data to prevent memory leaks
- **Statistics**: Get real-time statistics about rate limit usage

### 2. CORS Support
- **Origin validation**: Configure allowed origins (domains)
- **Method validation**: Specify which HTTP methods are allowed
- **Header validation**: Control which headers can be sent
- **Preflight handling**: Automatic handling of OPTIONS preflight requests
- **Credentials support**: Optional support for sending credentials

### 3. Unified Middleware
- **Combined processing**: Both rate limiting and CORS in one middleware
- **Easy configuration**: Simple API for updating settings
- **Production ready**: Optimized for high-performance scenarios

## Configuration

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

## Security Considerations

1. **Rate Limiting**: Helps prevent abuse and DoS attacks
2. **CORS**: Prevents unauthorized cross-origin requests
3. **Origin Validation**: Always validate allowed origins in production
4. **Header Control**: Limit allowed headers to reduce attack surface
5. **Credentials**: Only enable credentials when necessary

## Best Practices

1. **Production Configuration**: Use stricter limits in production
2. **Monitor Statistics**: Regularly check rate limit statistics
3. **Whitelist Origins**: Don't use "*" for origins in production
4. **Regular Cleanup**: The system automatically cleans up old data
5. **Logging**: Monitor logs for blocked requests and patterns

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

// Customize for specific requirements
auto corsConfig = authMiddleware->getCorsConfig();
corsConfig.allowedOrigins = {"https://yourdomain.com", "https://api.yourdomain.com"};
corsConfig.allowCredentials = true;
authMiddleware->updateCorsConfig(corsConfig);

auto rateLimitConfig = authMiddleware->getRateLimiterConfig();
rateLimitConfig.maxRequests = 60; // 1 request per second average
authMiddleware->updateRateLimiterConfig(rateLimitConfig);
```

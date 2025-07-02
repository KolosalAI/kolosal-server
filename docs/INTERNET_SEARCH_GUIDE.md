# Internet Search Guide

This guide explains how to use the Kolosal Server's internet search functionality, which provides a wrapper around SearXNG for web searches.

## Overview

The internet search endpoint allows you to perform web searches through your server, acting as a proxy to a SearXNG instance. This is useful for integrating web search capabilities into AI applications while maintaining control over the search backend.

## Configuration

### YAML Configuration

Add the following section to your `config.yaml` file:

```yaml
search:
  enabled: true                        # Enable/disable internet search endpoint
  searxng_url: "http://localhost:4000" # SearXNG instance URL
  timeout: 30                         # Request timeout in seconds
  max_results: 20                     # Maximum number of search results
  default_engine: ""                  # Default search engine (empty = SearXNG default)
  api_key: ""                         # Optional API key for authentication
  enable_safe_search: true            # Enable safe search by default
  default_format: "json"              # Default output format (json, xml, csv, rss)
  default_language: "en"              # Default search language
  default_category: "general"         # Default search category
```

### Command Line Configuration

You can also configure the search functionality using command line arguments:

```bash
# Enable search
kolosal-server --enable-search --searxng-url "http://localhost:4000"

# Configure search parameters
kolosal-server --enable-search \
  --search-url "http://your-searxng-instance:8080" \
  --search-timeout 60 \
  --search-max-results 50 \
  --search-language "en" \
  --search-category "general" \
  --search-safe-search

# Disable search
kolosal-server --disable-search
```

## API Usage

### Endpoint

The search functionality is available at these endpoints:
- `GET /internet_search`
- `POST /internet_search`
- `GET /v1/internet_search`
- `POST /v1/internet_search`
- `GET /search`
- `POST /search`

### Request Parameters

#### POST Request (JSON Body)

```json
{
  "query": "artificial intelligence",     // Required: Search query
  "engines": "google,bing",              // Optional: Comma-separated list of engines
  "categories": "general,news",          // Optional: Comma-separated list of categories
  "language": "en",                      // Optional: Search language
  "format": "json",                      // Optional: Output format (json, xml, csv, rss)
  "results": 10,                         // Optional: Number of results (1-100)
  "safe_search": true,                   // Optional: Enable safe search
  "timeout": 30                          // Optional: Request timeout (1-120 seconds)
}
```

#### GET Request (Query Parameters)

```
GET /internet_search?q=artificial%20intelligence&engines=google,bing&results=10&lang=en
```

### Request Parameters Reference

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `query` or `q` | string | Yes | - | The search query |
| `engines` | string | No | SearXNG default | Comma-separated list of search engines |
| `categories` | string | No | `general` | Comma-separated list of search categories |
| `language` or `lang` | string | No | `en` | Search language code |
| `format` | string | No | `json` | Output format: `json`, `xml`, `csv`, `rss` |
| `results` | integer | No | 20 | Number of results to return (1-100) |
| `safe_search` or `safesearch` | boolean | No | `true` | Enable safe search filtering |
| `timeout` | integer | No | 30 | Request timeout in seconds (1-120) |

### Response Format

#### Successful Response (JSON format)

The response format depends on the SearXNG instance configuration and the requested format. For JSON format, you'll typically receive:

```json
{
  "query": "artificial intelligence",
  "number_of_results": 10,
  "results": [
    {
      "title": "Artificial Intelligence - Wikipedia",
      "url": "https://en.wikipedia.org/wiki/Artificial_intelligence",
      "content": "Artificial intelligence (AI) is intelligence demonstrated by machines...",
      "engine": "wikipedia",
      "score": 100,
      "category": "general"
    }
  ],
  "suggestions": ["machine learning", "deep learning"],
  "infobox": {
    "title": "Artificial Intelligence",
    "content": "..."
  }
}
```

#### Error Response

```json
{
  "error": {
    "type": "invalid_request",
    "message": "Query parameter is required"
  }
}
```

### Example Requests

#### Simple Search

```bash
curl -X POST http://localhost:8080/internet_search \
  -H "Content-Type: application/json" \
  -d '{"query": "machine learning tutorials"}'
```

#### Advanced Search

```bash
curl -X POST http://localhost:8080/internet_search \
  -H "Content-Type: application/json" \
  -d '{
    "query": "climate change 2024",
    "engines": "google,bing,duckduckgo",
    "categories": "news,science",
    "language": "en",
    "results": 15,
    "safe_search": true
  }'
```

#### GET Request

```bash
curl "http://localhost:8080/internet_search?q=python%20programming&results=5&lang=en"
```

## SearXNG Configuration

You need a running SearXNG instance to use this feature. Here's how to set it up:

### Docker Installation

```bash
# Pull and run SearXNG
docker run -d \
  --name searxng \
  -p 4000:8080 \
  -v $(pwd)/searxng:/etc/searxng \
  searxng/searxng:latest
```

### Docker Compose

```yaml
version: '3.8'
services:
  searxng:
    image: searxng/searxng:latest
    container_name: searxng
    ports:
      - "4000:8080"
    volumes:
      - ./searxng:/etc/searxng
    environment:
      - SEARXNG_SECRET=your-secret-key-here
```

### Configuration

Create a `settings.yml` file for SearXNG:

```yaml
use_default_settings: true
server:
  port: 8080
  bind_address: "0.0.0.0"
  secret_key: "your-secret-key-here"
  
ui:
  static_use_hash: true
  
search:
  safe_search: 1
  autocomplete: "google"
  default_lang: "en"
  
engines:
  - name: google
    disabled: false
  - name: bing
    disabled: false
  - name: duckduckgo
    disabled: false
```

## Supported Search Engines

The available search engines depend on your SearXNG configuration. Common engines include:

- **Web Search**: google, bing, duckduckgo, yahoo, yandex
- **News**: google news, bing news, reuters, cnn, bbc
- **Images**: google images, bing images, flickr, unsplash
- **Videos**: youtube, vimeo, dailymotion
- **Academic**: google scholar, arxiv, crossref, pubmed
- **Maps**: google maps, openstreetmap
- **Shopping**: amazon, ebay, 1337x
- **Social**: reddit, twitter, mastodon

## Search Categories

Common search categories include:

- `general` - General web search
- `images` - Image search
- `videos` - Video search
- `news` - News search
- `map` - Map/location search
- `music` - Music search
- `it` - IT/programming search
- `science` - Scientific search
- `social media` - Social media search

## Language Codes

Use standard ISO 639-1 language codes:

- `en` - English
- `es` - Spanish
- `fr` - French
- `de` - German
- `zh` - Chinese
- `ja` - Japanese
- `ru` - Russian
- `pt` - Portuguese
- `it` - Italian
- `ar` - Arabic

## Output Formats

### JSON (default)
Structured data format, ideal for programmatic access.

### XML
XML format compatible with RSS/Atom readers.

### CSV
Comma-separated values for data analysis.

### RSS
RSS feed format for feed readers.

## Security Considerations

1. **Rate Limiting**: The search endpoint respects your server's rate limiting configuration.

2. **Safe Search**: Safe search is enabled by default to filter inappropriate content.

3. **Input Validation**: All search queries are validated to prevent injection attacks.

4. **Timeout Limits**: Request timeouts prevent long-running searches from consuming resources.

5. **Result Limits**: Maximum result count is enforced to prevent excessive resource usage.

## Thread Safety and Concurrency

The internet search implementation is fully thread-safe and supports concurrent requests:

- **Worker Threads**: Uses dedicated worker threads for HTTP requests
- **Connection Pooling**: Efficiently manages connections to the SearXNG instance
- **Async Processing**: Non-blocking request handling
- **Queue Management**: Thread-safe request queuing system

## Error Handling

Common error types and their meanings:

| Error Type | Description | HTTP Status |
|------------|-------------|-------------|
| `feature_disabled` | Search is not enabled | 503 |
| `invalid_request` | Invalid parameters | 400 |
| `timeout` | Search request timed out | 504 |
| `search_failed` | SearXNG returned an error | 502 |
| `internal_error` | Server internal error | 500 |

## Performance Optimization

1. **Caching**: Consider implementing caching in your SearXNG instance for frequently searched terms.

2. **Connection Pooling**: The server uses connection pooling to efficiently handle multiple requests.

3. **Timeout Configuration**: Adjust timeout values based on your SearXNG instance performance.

4. **Result Limiting**: Use appropriate result limits to balance between usefulness and performance.

## Monitoring and Logging

The search endpoint provides detailed logging:

```
[INFO] [Thread 12345] Received internet search request
[INFO] Making search request to: http://localhost:4000/search?q=example&format=json
[INFO] [Thread 12345] Search request completed successfully
```

## Integration Examples

### Python Client

```python
import requests
import json

def search_web(query, max_results=10):
    url = "http://localhost:8080/internet_search"
    payload = {
        "query": query,
        "results": max_results,
        "format": "json"
    }
    
    response = requests.post(url, json=payload)
    if response.status_code == 200:
        return response.json()
    else:
        print(f"Search failed: {response.status_code}")
        return None

# Usage
results = search_web("machine learning", 5)
if results:
    for result in results.get("results", []):
        print(f"Title: {result['title']}")
        print(f"URL: {result['url']}")
        print(f"Content: {result['content'][:100]}...")
        print()
```

### JavaScript/Node.js Client

```javascript
async function searchWeb(query, maxResults = 10) {
    const response = await fetch('http://localhost:8080/internet_search', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            query: query,
            results: maxResults,
            format: 'json'
        })
    });
    
    if (response.ok) {
        return await response.json();
    } else {
        console.error('Search failed:', response.status);
        return null;
    }
}

// Usage
searchWeb('artificial intelligence', 5)
    .then(results => {
        if (results) {
            results.results.forEach(result => {
                console.log(`Title: ${result.title}`);
                console.log(`URL: ${result.url}`);
                console.log(`Content: ${result.content.substring(0, 100)}...`);
                console.log();
            });
        }
    });
```

### cURL Examples

```bash
# Basic search
curl -X POST http://localhost:8080/internet_search \
  -H "Content-Type: application/json" \
  -d '{"query": "OpenAI GPT-4"}'

# Search with specific engines
curl -X POST http://localhost:8080/internet_search \
  -H "Content-Type: application/json" \
  -d '{"query": "climate change", "engines": "google,bing", "results": 20}'

# News search
curl -X POST http://localhost:8080/internet_search \
  -H "Content-Type: application/json" \
  -d '{"query": "tech news", "categories": "news", "language": "en"}'

# GET request
curl "http://localhost:8080/internet_search?q=python&results=5"
```

## Troubleshooting

### Common Issues

1. **"Feature disabled" error**
   - Enable search in configuration: `search.enabled: true`
   - Restart the server after configuration changes

2. **"Connection refused" errors**
   - Verify SearXNG is running and accessible
   - Check the `searxng_url` configuration
   - Ensure firewall allows connections

3. **Timeout errors**
   - Increase timeout values in configuration
   - Check SearXNG instance performance
   - Verify network connectivity

4. **Empty results**
   - Check SearXNG engine configuration
   - Verify search engines are enabled
   - Try different search categories

5. **Invalid format errors**
   - Use supported formats: json, xml, csv, rss
   - Check SearXNG supports the requested format

### Debug Mode

Enable debug logging to troubleshoot issues:

```yaml
logging:
  level: "DEBUG"
```

This will provide detailed information about search requests and responses.

## Best Practices

1. **Use Appropriate Timeouts**: Set reasonable timeout values based on your use case.

2. **Limit Result Count**: Request only the number of results you actually need.

3. **Cache Results**: Implement client-side caching for frequently searched terms.

4. **Handle Errors Gracefully**: Always check for errors and provide fallback behavior.

5. **Use Specific Engines**: Specify engines when you know what type of content you're looking for.

6. **Respect Rate Limits**: Don't overwhelm the search service with too many concurrent requests.

7. **Monitor Performance**: Keep track of search response times and error rates.

---

For more information about SearXNG configuration and capabilities, visit the [SearXNG documentation](https://docs.searxng.org/).

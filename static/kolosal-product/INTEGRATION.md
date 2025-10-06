# Frontend-Backend Integration Guide

## Overview

This document describes how the Kolosal frontend is integrated with the backend server.

## Architecture

### Frontend Structure
```
static/kolosal-product/
├── component/          # Reusable UI components
│   ├── api-helper.js   # API utility functions
│   ├── badge.js
│   ├── head.js         # Head/favicon initialization
│   ├── input.js
│   ├── popup.js
│   ├── select.js
│   └── sidebar.js
├── config.js           # API configuration
├── discover/           # Model discovery page
├── model/             # Model management page
├── playground/        # Chat playground page
├── retrieve/          # Document retrieval pages
│   └── script/
│       ├── collection.js
│       ├── search.js
│       ├── status.js
│       └── upload.js
├── styles/            # Global styles
├── favicon.ico        # Site favicon
├── logo.png           # Site logo
├── index.html         # Main playground page
└── test-integration.html  # Backend integration test page
```

### Backend Serving

The backend serves all static files from the `static/kolosal-product/` directory through the `UIRoute` class in `src/routes/ui_routes.cpp`.

## Configuration

### API Base URL

The frontend automatically detects the API base URL from `window.location.origin`, allowing it to work in both development and production environments without configuration changes.

**File:** `static/kolosal-product/config.js`

```javascript
export const API_BASE_URL = window.location.origin;

export function getApiUrl(endpoint) {
    const cleanEndpoint = endpoint.startsWith('/') ? endpoint.slice(1) : endpoint;
    return `${API_BASE_URL}/${cleanEndpoint}`;
}
```

### Usage in Frontend Code

```javascript
import { getApiUrl } from './config.js';
import { apiGet, apiPost } from './component/api-helper.js';

// Simple fetch
const data = await apiGet('health');

// POST with JSON
const result = await apiPost('add_documents', { documents: [...] });
```

## API Helper Module

The `api-helper.js` module provides:

1. **Error Handling**: Automatic error logging and user notifications
2. **Request Logging**: Console logging of all API calls
3. **Convenience Methods**: `apiGet()`, `apiPost()`, `apiPostFormData()`
4. **Health Checks**: `checkBackendHealth()`
5. **Notifications**: `showNotification(message, type)`

## Favicon Setup

The favicon is configured to use the `favicon.ico` file from the root of the static directory:

- **Source**: `assets/icon.ico`
- **Deployed**: `static/kolosal-product/favicon.ico`
- **Loaded by**: `component/head.js`

The `HeadInitiate()` function in `head.js` sets up:
- Site favicon (multiple sizes supported in .ico format)
- Remix Icon font from CDN

## Available Routes

### Frontend Pages
- `/` or `/playground` - Main chat playground
- `/discover` - Model discovery and download
- `/model` - Installed models management
- `/retrieve` - Document retrieval homepage
- `/retrieve/upload` - Document upload
- `/retrieve/search` - Document search
- `/retrieve/collection` - Document collection management
- `/test-integration` - Backend integration test page

### API Endpoints
All API endpoints are accessible through the same base URL:

- `GET /health` - Server health status
- `GET /models` - List loaded models
- `GET /engines` - List available engines
- `GET /list_documents` - List documents in collection
- `POST /add_documents` - Add documents to collection
- `POST /retrieve` - Search/retrieve documents
- `POST /chunking` - Chunk documents
- `POST /parse_pdf`, `/parse_doc`, etc. - Parse documents

## Testing the Integration

Visit `/test-integration` in your browser to run automated tests that verify:

1. ✅ Configuration is correct
2. ✅ Backend is reachable
3. ✅ All major API endpoints respond correctly
4. ✅ Response data format is valid

Example: `http://localhost:8080/test-integration`

## Error Handling

### Frontend Error Handling

All API calls use try-catch blocks with proper error logging:

```javascript
try {
    const data = await apiGet('health');
    // Handle success
} catch (error) {
    console.error('[Component] Error:', error);
    showNotification('Failed to load data', 'error');
    // Handle error state
}
```

### Backend Error Responses

The backend serves:
- **404 Page**: Custom styled 404 page for missing resources
- **CORS Headers**: Proper CORS headers for cross-origin requests
- **Content-Type**: Correct content types for all file types

## Development Tips

### 1. Check Backend is Running
```bash
# Backend should be running on http://localhost:8080
curl http://localhost:8080/health
```

### 2. View Console Logs
Open browser DevTools (F12) to see:
- API request/response logs
- Configuration information
- Error messages

### 3. Test API Endpoints
Use the integration test page or tools like:
```bash
# Test health endpoint
curl http://localhost:8080/health

# Test models endpoint
curl http://localhost:8080/models

# Test with POST
curl -X POST http://localhost:8080/list_documents \
  -H "Content-Type: application/json"
```

### 4. Check Static Files
Verify static files are served correctly:
```bash
curl http://localhost:8080/favicon.ico
curl http://localhost:8080/component/head.js
```

## Common Issues and Solutions

### Issue: Favicon Not Loading

**Solution**: 
1. Verify `favicon.ico` exists in `static/kolosal-product/`
2. Copy from assets: `Copy-Item assets\icon.ico static\kolosal-product\favicon.ico`
3. Clear browser cache (Ctrl+F5)

### Issue: API Calls Failing

**Symptoms**: "Failed to connect to backend server" notification

**Solutions**:
1. Check backend is running: `curl http://localhost:8080/health`
2. Check browser console for CORS errors
3. Verify API_BASE_URL in config.js
4. Run integration tests at `/test-integration`

### Issue: 404 Errors for Static Assets

**Solutions**:
1. Verify file exists in `static/kolosal-product/`
2. Check file path in HTML/JS matches actual location
3. Check `ui_routes.cpp` includes the file extension in `isStaticAsset()`

### Issue: Styles Not Loading

**Solutions**:
1. Check CSS files are in `static/kolosal-product/styles/`
2. Verify `<link>` tags in HTML have correct paths
3. Check content-type is `text/css` in network tab

## Building and Deployment

### Development
The frontend is served directly from the `static/kolosal-product/` directory. Any changes to files are immediately reflected (after browser refresh).

### Production
1. Copy assets to static directory:
```bash
Copy-Item assets\icon.ico static\kolosal-product\favicon.ico
Copy-Item assets\logo.png static\kolosal-product\logo.png
```

2. Build the backend:
```bash
cd build
cmake ..
cmake --build . --config Release
```

3. Run the server:
```bash
.\build\Release\kolosal_server_exe.exe
```

## Security Considerations

1. **Path Traversal Protection**: The backend validates all file paths to prevent directory traversal attacks
2. **CORS**: Configured for development; adjust for production domains
3. **Content-Type Headers**: Proper content types prevent MIME sniffing attacks
4. **Input Validation**: All API inputs are validated on the backend

## Browser Support

The frontend uses modern JavaScript (ES6 modules) and CSS features. Supported browsers:
- Chrome/Edge 90+
- Firefox 88+
- Safari 14+

## Performance Optimization

1. **Caching**: Static assets have cache headers set:
   - Images/fonts: 1 week
   - CSS/JS: 1 hour
   - HTML: 5 minutes

2. **ETags**: Response ETags enable conditional requests

3. **Lazy Loading**: Components are loaded on-demand

## Contributing

When adding new frontend features:

1. Use `getApiUrl()` for all API endpoints
2. Use `apiGet()`, `apiPost()` from `api-helper.js`
3. Add proper error handling with `showNotification()`
4. Test with the integration test page
5. Update this documentation

## Related Files

- `src/routes/ui_routes.cpp` - Backend static file serving
- `src/routes/ui_routes.hpp` - Route interface
- `static/kolosal-product/config.js` - API configuration
- `static/kolosal-product/component/api-helper.js` - API utilities
- `static/kolosal-product/component/head.js` - Favicon/head setup

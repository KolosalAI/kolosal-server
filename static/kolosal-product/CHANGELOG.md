# API Configuration Update - Change Summary

## Date
October 6, 2025

## Changes Made

### Overview
Updated the Kolosal Product frontend to connect to the local Kolosal Server instead of the external API (https://api.kolosal.ai).

### New Files Created

1. **`config.js`**
   - Centralized API configuration file
   - Contains `API_BASE_URL` constant set to `http://localhost:8080`
   - Provides `getApiUrl()` helper function for building endpoint URLs
   - Makes it easy to change the API URL in one place

2. **`README.md`**
   - Documentation for the frontend application
   - Instructions on how to change the API URL
   - Overview of features and file structure

### Modified Files

All API endpoints have been updated from `https://api.kolosal.ai` to use the local server via the `getApiUrl()` function:

1. **`retrieve/script/status.js`**
   - `/health` endpoint
   - `/list_documents` endpoint

2. **`retrieve/script/search.js`**
   - `/retrieve` endpoint

3. **`retrieve/script/collection.js`**
   - `/list_documents` endpoint
   - `/info_documents` endpoint

4. **`model/script.js`**
   - `/models` endpoint

5. **`retrieve/script/upload.js`**
   - `/parse_{docType}` endpoints
   - `/v1/convert/file` endpoint
   - `/chunking` endpoint
   - `/add_documents` endpoint

### API Endpoints Updated

The following endpoints now point to `http://localhost:8080`:

- `GET /health` - Server health check
- `GET /list_documents` - List all documents
- `POST /info_documents` - Get document information
- `POST /retrieve` - Retrieve documents by query
- `GET /models` - List available models
- `POST /parse_pdf` - Parse PDF documents
- `POST /parse_doc` - Parse DOC documents
- `POST /parse_pptx` - Parse PPTX documents
- `POST /parse_xlx` - Parse Excel documents
- `POST /parse_html` - Parse HTML documents
- `POST /v1/convert/file` - Docling file conversion
- `POST /chunking` - Chunk documents
- `POST /add_documents` - Add documents to collection

### Configuration

The server is expected to run on `http://localhost:8080` as configured in `configs/config.yaml`.

To change the API URL, simply edit the `API_BASE_URL` in `static/kolosal-product/config.js`:

```javascript
export const API_BASE_URL = 'http://localhost:8080';
```

### Benefits

1. **Centralized Configuration**: All API URLs are now managed in one place
2. **Easy Maintenance**: Changing the API URL requires editing only one file
3. **Local Development**: Frontend now works seamlessly with the local server
4. **Better Organization**: Clear separation between configuration and logic
5. **Documentation**: README provides clear instructions for developers

### Testing Recommendations

1. Start the Kolosal Server: `.\build\Release\kolosal-server.exe --config configs/config.yaml`
2. Open the frontend in a browser
3. Verify that all features work correctly:
   - Model status displays correctly
   - Document list loads
   - Search/retrieval works
   - Document upload and parsing works
   - Collection management works

### Notes

- The HuggingFace API calls in `discover/script.js` were intentionally left unchanged as they connect to external HuggingFace services, not the Kolosal API
- All JavaScript files now import and use the centralized configuration
- CORS is enabled in the server configuration to allow browser access

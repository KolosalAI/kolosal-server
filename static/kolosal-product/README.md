# Kolosal Product Frontend

This is the frontend interface for the Kolosal Server.

## Configuration

The API endpoint is configured in `config.js`. By default, it points to `http://localhost:8080`.

### Changing the API URL

To change the API server URL, edit the `API_BASE_URL` constant in `config.js`:

```javascript
export const API_BASE_URL = 'http://localhost:8080';
```

For example, to point to a production server:

```javascript
export const API_BASE_URL = 'https://api.kolosal.ai';
```

Or to use a different port:

```javascript
export const API_BASE_URL = 'http://localhost:3000';
```

## Running the Application

1. Start the Kolosal Server (default port: 8080)
2. Open `index.html` in your browser or serve the static files through a web server
3. The frontend will automatically connect to the configured API endpoint

## Features

- **Model Management**: View and manage loaded models
- **Document Retrieval**: Search and retrieve documents from the vector database
- **Document Upload**: Parse and add documents to the collection
- **Collection Management**: View and manage document collections

## File Structure

- `config.js` - API configuration
- `index.html` - Main entry point
- `component/` - Reusable UI components
- `model/` - Model management interface
- `retrieve/` - Document retrieval and management interface
- `discover/` - Model discovery interface
- `playground/` - Interactive playground
- `styles/` - CSS stylesheets

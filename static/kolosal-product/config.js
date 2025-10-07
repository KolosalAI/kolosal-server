// API Configuration
// Automatically detect the API base URL from the current location
// This allows the frontend to work seamlessly whether served from the same server or a different one
export const API_BASE_URL = window.location.origin;

// Helper function to build full API URLs
export function getApiUrl(endpoint) {
    // Remove leading slash if present to avoid double slashes
    const cleanEndpoint = endpoint.startsWith('/') ? endpoint.slice(1) : endpoint;
    return `${API_BASE_URL}/${cleanEndpoint}`;
}

// Export configuration information for debugging
export const CONFIG_INFO = {
    apiBaseUrl: API_BASE_URL,
    environment: API_BASE_URL.includes('localhost') || API_BASE_URL.includes('127.0.0.1') ? 'development' : 'production'
};

// Log configuration on module load for debugging
console.log('[Kolosal Config] API Base URL:', API_BASE_URL);
console.log('[Kolosal Config] Environment:', CONFIG_INFO.environment);

/**
 * API Helper Module
 * Provides utility functions for making API calls with proper error handling
 */

import { getApiUrl, CONFIG_INFO } from '../config.js';

/**
 * Makes a fetch request with error handling and retries
 * @param {string} endpoint - The API endpoint
 * @param {Object} options - Fetch options
 * @returns {Promise<Response>}
 */
export async function apiFetch(endpoint, options = {}) {
    const url = getApiUrl(endpoint);
    
    // Add default headers
    const defaultHeaders = {
        'Content-Type': 'application/json',
    };
    
    options.headers = {
        ...defaultHeaders,
        ...options.headers
    };

    try {
        console.log(`[API] ${options.method || 'GET'} ${url}`);
        const response = await fetch(url, options);
        
        if (!response.ok) {
            console.error(`[API Error] ${response.status} ${response.statusText} - ${url}`);
        } else {
            console.log(`[API Success] ${response.status} - ${url}`);
        }
        
        return response;
    } catch (error) {
        console.error(`[API Exception] ${error.message} - ${url}`);
        throw error;
    }
}

/**
 * Makes a GET request
 * @param {string} endpoint - The API endpoint
 * @returns {Promise<any>}
 */
export async function apiGet(endpoint) {
    const response = await apiFetch(endpoint, { method: 'GET' });
    if (!response.ok) {
        throw new Error(`API Error: ${response.status} ${response.statusText}`);
    }
    return await response.json();
}

/**
 * Makes a POST request
 * @param {string} endpoint - The API endpoint
 * @param {Object} data - The data to send
 * @returns {Promise<any>}
 */
export async function apiPost(endpoint, data) {
    const response = await apiFetch(endpoint, {
        method: 'POST',
        body: JSON.stringify(data)
    });
    if (!response.ok) {
        throw new Error(`API Error: ${response.status} ${response.statusText}`);
    }
    return await response.json();
}

/**
 * Makes a POST request with FormData
 * @param {string} endpoint - The API endpoint
 * @param {FormData} formData - The form data to send
 * @returns {Promise<any>}
 */
export async function apiPostFormData(endpoint, formData) {
    const response = await apiFetch(endpoint, {
        method: 'POST',
        headers: {}, // Let browser set Content-Type for FormData
        body: formData
    });
    if (!response.ok) {
        throw new Error(`API Error: ${response.status} ${response.statusText}`);
    }
    return await response.json();
}

/**
 * Checks if the backend is healthy
 * @returns {Promise<boolean>}
 */
export async function checkBackendHealth() {
    try {
        const data = await apiGet('health');
        return data.status === 'healthy';
    } catch (error) {
        console.error('[Health Check] Backend is not responding:', error);
        return false;
    }
}

/**
 * Shows a notification to the user
 * @param {string} message - The message to show
 * @param {string} type - The type of notification (success, error, warning, info)
 */
export function showNotification(message, type = 'info') {
    // Create notification element if it doesn't exist
    let container = document.getElementById('notification-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'notification-container';
        container.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            z-index: 10000;
            display: flex;
            flex-direction: column;
            gap: 10px;
        `;
        document.body.appendChild(container);
    }

    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.style.cssText = `
        padding: 15px 20px;
        border-radius: 8px;
        background-color: ${type === 'error' ? '#FF2723' : type === 'success' ? '#00C853' : type === 'warning' ? '#FFA000' : '#0370FF'};
        color: white;
        box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        max-width: 400px;
        font-size: 14px;
        animation: slideIn 0.3s ease-out;
    `;
    notification.textContent = message;

    container.appendChild(notification);

    // Auto-remove after 5 seconds
    setTimeout(() => {
        notification.style.animation = 'slideOut 0.3s ease-out';
        setTimeout(() => notification.remove(), 300);
    }, 5000);
}

// Add animation styles if not present
if (!document.getElementById('notification-styles')) {
    const style = document.createElement('style');
    style.id = 'notification-styles';
    style.textContent = `
        @keyframes slideIn {
            from {
                transform: translateX(400px);
                opacity: 0;
            }
            to {
                transform: translateX(0);
                opacity: 1;
            }
        }
        @keyframes slideOut {
            from {
                transform: translateX(0);
                opacity: 1;
            }
            to {
                transform: translateX(400px);
                opacity: 0;
            }
        }
    `;
    document.head.appendChild(style);
}

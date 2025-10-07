import { HeadInitiate } from "../component/head.js";
import { Select } from "../component/select.js";
import { Sidebar } from "../component/sidebar.js";
import { getApiUrl } from "../config.js";

// State management
const state = {
    currentChatId: null,
    chats: {},
    selectedModel: null,
    availableModels: [],
    isStreaming: false,
    // Generation parameters
    temperature: 0.7,
    maxLength: 2048,
    topP: 0.9,
    systemInstruction: ""
};

// Initialize chat storage from localStorage
function initChatStorage() {
    const stored = localStorage.getItem('kolosal_chats');
    if (stored) {
        try {
            state.chats = JSON.parse(stored);
        } catch (e) {
            console.error('Failed to parse stored chats:', e);
            state.chats = {};
        }
    }
    
    // If no chats exist, create a default one
    if (Object.keys(state.chats).length === 0) {
        createNewChat();
    } else {
        // Load the first chat
        const firstChatId = Object.keys(state.chats)[0];
        loadChat(firstChatId);
    }
}

// Save chats to localStorage
function saveChats() {
    try {
        localStorage.setItem('kolosal_chats', JSON.stringify(state.chats));
    } catch (e) {
        console.error('Failed to save chats:', e);
    }
}

// Create a new chat
function createNewChat() {
    const chatId = 'chat_' + Date.now();
    const chat = {
        id: chatId,
        title: 'New Chat',
        messages: [],
        createdAt: new Date().toISOString(),
        model: state.selectedModel
    };
    
    state.chats[chatId] = chat;
    state.currentChatId = chatId;
    saveChats();
    renderChatList();
    renderMessages();
}

// Load a specific chat
function loadChat(chatId) {
    if (state.chats[chatId]) {
        state.currentChatId = chatId;
        renderMessages();
        renderChatList();
    }
}

// Delete a chat
function deleteChat(chatId) {
    if (confirm('Are you sure you want to delete this chat?')) {
        delete state.chats[chatId];
        saveChats();
        
        // If deleted current chat, load another or create new
        if (state.currentChatId === chatId) {
            const remainingChats = Object.keys(state.chats);
            if (remainingChats.length > 0) {
                loadChat(remainingChats[0]);
            } else {
                createNewChat();
            }
        } else {
            renderChatList();
        }
    }
}

// Render chat list in sidebar
function renderChatList() {
    const chatList = document.querySelector('.chat-list');
    if (!chatList) return;
    
    chatList.innerHTML = '';
    
    const sortedChats = Object.values(state.chats).sort((a, b) => 
        new Date(b.createdAt) - new Date(a.createdAt)
    );
    
    sortedChats.forEach(chat => {
        const item = document.createElement('div');
        item.className = `list-item${chat.id === state.currentChatId ? ' active' : ''}`;
        
        const title = chat.title || 'New Chat';
        item.innerHTML = `
            <h2 class="text-14px reguler">${title}</h2>
            <button class="delete-chat-btn"><i class="ri-delete-bin-line"></i></button>
        `;
        
        item.addEventListener('click', (e) => {
            if (!e.target.closest('.delete-chat-btn')) {
                loadChat(chat.id);
            }
        });
        
        const deleteBtn = item.querySelector('.delete-chat-btn');
        deleteBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            deleteChat(chat.id);
        });
        
        chatList.appendChild(item);
    });
}

// Update chat title based on first message
function updateChatTitle(chatId) {
    const chat = state.chats[chatId];
    if (chat && chat.messages.length > 0 && chat.title === 'New Chat') {
        const firstMessage = chat.messages[0].content;
        chat.title = firstMessage.substring(0, 50) + (firstMessage.length > 50 ? '...' : '');
        saveChats();
        renderChatList();
    }
}

// Render messages in current chat
function renderMessages() {
    const boxBody = document.querySelector('.box-body');
    if (!boxBody) return;
    
    const currentChat = state.chats[state.currentChatId];
    if (!currentChat || currentChat.messages.length === 0) {
        // Show blank state
        boxBody.innerHTML = `
            <div class="blank">
                <img src="https://res.cloudinary.com/dh21tvktj/image/upload/v1758625415/kolosal-blank-chat_aeewug.png" alt="">
                <h2 class="text-32px semibold">Your first chat starts here</h2>
                <h3 class="text-32px semibold">What's on your mind?</h3>
            </div>
        `;
        return;
    }
    
    boxBody.innerHTML = '<div class="bubble"></div>';
    const bubbleContainer = boxBody.querySelector('.bubble');
    
    currentChat.messages.forEach(msg => {
        if (msg.role === 'user') {
            const userBubble = document.createElement('div');
            userBubble.className = 'bubble-user';
            userBubble.innerHTML = `
                <div class="bubble-user-wrapper">
                    <p class="article-text reguler">${escapeHtml(msg.content)}</p>
                </div>
            `;
            bubbleContainer.appendChild(userBubble);
        } else if (msg.role === 'assistant') {
            const botBubble = document.createElement('div');
            botBubble.className = 'bubble-bot';
            botBubble.innerHTML = `
                <div class="bubble-bot-wrapper">
                    <p class="article-text reguler">${escapeHtml(msg.content)}</p>
                </div>
            `;
            bubbleContainer.appendChild(botBubble);
        }
    });
    
    // Scroll to bottom
    boxBody.scrollTop = boxBody.scrollHeight;
}

// Escape HTML to prevent XSS
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Fetch available models from server
async function fetchModels() {
    try {
        const response = await fetch(getApiUrl('models'));
        if (!response.ok) {
            throw new Error('Failed to fetch models');
        }
        
        const data = await response.json();
        state.availableModels = data.models || [];
        
        // Filter for LLM models only
        const llmModels = state.availableModels.filter(m => 
            m.model_type?.toLowerCase() === 'llm' && 
            m.status === 'loaded'
        );
        
        if (llmModels.length > 0 && !state.selectedModel) {
            state.selectedModel = llmModels[0].model_id;
        }
        
        renderModelSelector(llmModels);
    } catch (error) {
        console.error('Error fetching models:', error);
        showError('Failed to load models. Please check your connection.');
    }
}

// Render model selector dropdown
function renderModelSelector(models) {
    const selectTrigger = document.querySelector('.select-trigger h2');
    const selectList = document.querySelector('.select-list');
    
    if (!selectTrigger || !selectList) return;
    
    if (models.length === 0) {
        selectTrigger.textContent = 'No models available';
        selectList.innerHTML = '<div class="select-list-item"><h2 class="text-12px reguler">No loaded models</h2></div>';
        return;
    }
    
    // Set selected model text
    if (state.selectedModel) {
        selectTrigger.textContent = state.selectedModel;
    }
    
    // Populate dropdown
    selectList.innerHTML = '';
    models.forEach(model => {
        const item = document.createElement('div');
        item.className = 'select-list-item';
        item.innerHTML = `<h2 class="text-12px reguler">${model.model_id}</h2>`;
        item.addEventListener('click', () => {
            state.selectedModel = model.model_id;
            selectTrigger.textContent = model.model_id;
        });
        selectList.appendChild(item);
    });
}

// Send message to chat
async function sendMessage(message) {
    if (!message.trim() || state.isStreaming) return;
    if (!state.selectedModel) {
        showError('Please select a model first');
        return;
    }
    
    const currentChat = state.chats[state.currentChatId];
    if (!currentChat) return;
    
    // Add user message
    currentChat.messages.push({
        role: 'user',
        content: message
    });
    
    saveChats();
    updateChatTitle(state.currentChatId);
    renderMessages();
    
    // Clear input
    const textarea = document.querySelector('.box-action textarea');
    if (textarea) textarea.value = '';
    
    // Prepare messages for API
    const messages = [...currentChat.messages];
    
    // Add system instruction if provided
    if (state.systemInstruction.trim()) {
        messages.unshift({
            role: 'system',
            content: state.systemInstruction
        });
    }
    
    // Show loading indicator
    const boxBody = document.querySelector('.box-body');
    const bubbleContainer = boxBody?.querySelector('.bubble');
    const loadingBubble = document.createElement('div');
    loadingBubble.className = 'bubble-bot';
    loadingBubble.innerHTML = `
        <div class="bubble-bot-wrapper">
            <p class="article-text reguler">Thinking...</p>
        </div>
    `;
    bubbleContainer?.appendChild(loadingBubble);
    boxBody.scrollTop = boxBody.scrollHeight;
    
    state.isStreaming = true;
    
    try {
        const response = await fetch(getApiUrl('v1/chat/completions'), {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                model: state.selectedModel,
                messages: messages,
                temperature: state.temperature,
                max_tokens: state.maxLength,
                top_p: state.topP,
                stream: true
            })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        // Remove loading bubble
        loadingBubble.remove();
        
        // Create assistant message bubble
        const assistantBubble = document.createElement('div');
        assistantBubble.className = 'bubble-bot';
        assistantBubble.innerHTML = `
            <div class="bubble-bot-wrapper">
                <p class="article-text reguler"></p>
            </div>
        `;
        bubbleContainer?.appendChild(assistantBubble);
        const assistantText = assistantBubble.querySelector('p');
        
        let fullResponse = '';
        
        // Handle streaming response
        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            
            const chunk = decoder.decode(value);
            const lines = chunk.split('\n').filter(line => line.trim() !== '');
            
            for (const line of lines) {
                if (line.startsWith('data: ')) {
                    const data = line.slice(6);
                    if (data === '[DONE]') continue;
                    
                    try {
                        const parsed = JSON.parse(data);
                        const content = parsed.choices?.[0]?.delta?.content;
                        if (content) {
                            fullResponse += content;
                            assistantText.textContent = fullResponse;
                            boxBody.scrollTop = boxBody.scrollHeight;
                        }
                    } catch (e) {
                        console.error('Error parsing SSE data:', e);
                    }
                }
            }
        }
        
        // Add assistant response to chat
        currentChat.messages.push({
            role: 'assistant',
            content: fullResponse
        });
        
        saveChats();
        
    } catch (error) {
        console.error('Error sending message:', error);
        loadingBubble.remove();
        showError('Failed to send message. Please try again.');
    } finally {
        state.isStreaming = false;
    }
}

// Show error message
function showError(message) {
    const boxBody = document.querySelector('.box-body');
    if (!boxBody) return;
    
    const errorBubble = document.createElement('div');
    errorBubble.className = 'bubble-bot';
    errorBubble.innerHTML = `
        <div class="bubble-bot-wrapper" style="background-color: #fee; border-color: #fcc;">
            <p class="article-text reguler" style="color: #c00;">${message}</p>
        </div>
    `;
    
    const bubbleContainer = boxBody.querySelector('.bubble');
    if (bubbleContainer) {
        bubbleContainer.appendChild(errorBubble);
        boxBody.scrollTop = boxBody.scrollHeight;
        
        // Remove error after 5 seconds
        setTimeout(() => errorBubble.remove(), 5000);
    }
}

// Setup event listeners
function setupEventListeners() {
    // New chat button
    const newChatBtn = document.querySelector('.chat-action .list-item');
    if (newChatBtn) {
        newChatBtn.addEventListener('click', () => {
            createNewChat();
        });
    }
    
    // Send button
    const sendBtn = document.querySelector('.box-action button');
    const textarea = document.querySelector('.box-action textarea');
    
    if (sendBtn && textarea) {
        sendBtn.addEventListener('click', () => {
            const message = textarea.value.trim();
            if (message) {
                sendMessage(message);
            }
        });
        
        // Send on Enter (Shift+Enter for new line)
        textarea.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                const message = textarea.value.trim();
                if (message) {
                    sendMessage(message);
                }
            }
        });
    }
    
    // Temperature slider
    const tempSlider = document.querySelector('.box-option-content:nth-child(1) .input-range');
    const tempValue = document.querySelector('.box-option-content:nth-child(1) .title p');
    if (tempSlider && tempValue) {
        tempSlider.min = 0;
        tempSlider.max = 2;
        tempSlider.step = 0.1;
        tempSlider.value = state.temperature;
        tempValue.textContent = state.temperature.toFixed(1);
        
        tempSlider.addEventListener('input', (e) => {
            state.temperature = parseFloat(e.target.value);
            tempValue.textContent = state.temperature.toFixed(1);
        });
    }
    
    // Max length slider
    const maxLenSlider = document.querySelector('.box-option-content:nth-child(2) .input-range');
    const maxLenValue = document.querySelector('.box-option-content:nth-child(2) .title p');
    if (maxLenSlider && maxLenValue) {
        maxLenSlider.min = 128;
        maxLenSlider.max = 4096;
        maxLenSlider.step = 128;
        maxLenSlider.value = state.maxLength;
        maxLenValue.textContent = state.maxLength;
        
        maxLenSlider.addEventListener('input', (e) => {
            state.maxLength = parseInt(e.target.value);
            maxLenValue.textContent = state.maxLength;
        });
    }
    
    // Top P slider
    const topPSlider = document.querySelector('.box-option-content:nth-child(3) .input-range');
    const topPValue = document.querySelector('.box-option-content:nth-child(3) .title p');
    if (topPSlider && topPValue) {
        topPSlider.min = 0;
        topPSlider.max = 1;
        topPSlider.step = 0.05;
        topPSlider.value = state.topP;
        topPValue.textContent = state.topP.toFixed(2);
        
        topPSlider.addEventListener('input', (e) => {
            state.topP = parseFloat(e.target.value);
            topPValue.textContent = state.topP.toFixed(2);
        });
    }
    
    // System instruction textarea
    const instructionTextarea = document.querySelector('.box-option-content:nth-child(4) .input-textarea');
    if (instructionTextarea) {
        instructionTextarea.value = state.systemInstruction;
        instructionTextarea.addEventListener('input', (e) => {
            state.systemInstruction = e.target.value;
        });
    }
}

function ToggleOption() {
    const buttons = document.querySelectorAll(".ButtonOption");
    const box = document.querySelector(".box-option"); 

    buttons.forEach(btn => {
        btn.addEventListener("click", () => {
        if (box.classList.contains("active")) {
            box.classList.remove("active");
            box.addEventListener("transitionend", () => {
            if (!box.classList.contains("active")) {
                box.style.display = "none";
            }
            }, { once: true });
        } else {
            box.style.display = "flex";
            requestAnimationFrame(() => {
            box.classList.add("active");
            });
        }
        });
    });
}

// Initialize application
async function init() {
    HeadInitiate();
    Sidebar();
    Select();
    ToggleOption();
    
    initChatStorage();
    setupEventListeners();
    await fetchModels();
}

// Start the application
init();
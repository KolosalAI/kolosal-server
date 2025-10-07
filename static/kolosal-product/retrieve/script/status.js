import { Badge } from "../../component/badge.js";
import { HeadInitiate } from "../../component/head.js";
import { Sidebar, SidebarAlt } from "../../component/sidebar.js";
import { apiGet, showNotification } from "../../component/api-helper.js";

async function ModelStatus() {
    try {
        const data = await apiGet('health');
        
        const setState = (id) => {
            const el = document.getElementById(id);
            if (!el) return;
            if (data.status === "healthy") {
                el.setAttribute("data-state", "success");
                el.setAttribute("data-text", "CONNECTED");
            } else {
                el.setAttribute("data-state", "error");
                el.setAttribute("data-text", "DISCONNECT");
            }
        };

        setState("LLMStatus");
        setState("EmbeddingStatus");
        setState("ParserStatus");
        setState("MarkItDownStatus");
        setState("DoclingStatus");
        setState("DocumentStatus");

        let llmCount = 0;
        let embeddingCount = 0;
        if (data.engines && Array.isArray(data.engines)) {
            data.engines.forEach(engine => {
                if (engine.engine_id.toLowerCase().includes("embedding")) {
                    embeddingCount++;
                } else {
                    llmCount++;
                }
            });
        }

        const llmCountEl = document.getElementById("LLMCount");
        const embeddingCountEl = document.getElementById("EmbeddingCount");
        if (llmCountEl) llmCountEl.textContent = llmCount;
        if (embeddingCountEl) embeddingCountEl.textContent = embeddingCount;

        Badge();
    } catch (error) {
        console.error('[ModelStatus] Failed to fetch health status:', error);
        showNotification('Failed to connect to backend server', 'error');
        
        // Set all to error state
        const errorIds = ["LLMStatus", "EmbeddingStatus", "ParserStatus", "MarkItDownStatus", "DoclingStatus", "DocumentStatus"];
        errorIds.forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.setAttribute("data-state", "error");
                el.setAttribute("data-text", "DISCONNECT");
            }
        });
        Badge();
    }
}

async function DocumentStatus() {
    try {
        const data = await apiGet('list_documents');
        
        const collectionNameEl = document.getElementById("CollectionName");
        const countDocumentEl = document.getElementById("CountDocument");
        if (collectionNameEl) { 
            collectionNameEl.textContent = data.collection_name || 'N/A'; 
        }
        if (countDocumentEl) { 
            countDocumentEl.textContent = data.total_count || 0; 
        }
    } catch (error) {
        console.error('[DocumentStatus] Failed to fetch document status:', error);
        const collectionNameEl = document.getElementById("CollectionName");
        const countDocumentEl = document.getElementById("CountDocument");
        if (collectionNameEl) collectionNameEl.textContent = 'Error';
        if (countDocumentEl) countDocumentEl.textContent = '0';
    }
}

HeadInitiate()
Sidebar();
SidebarAlt();
ModelStatus();
DocumentStatus();
Badge();
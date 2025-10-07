import { Badge } from "../component/badge.js";
import { HeadInitiate } from "../component/head.js";
import { Sidebar } from "../component/sidebar.js";
import { getApiUrl } from "../config.js";

function RenderModel(models) {
    const container = document.querySelector('.model-list');
    if (!container) return;
    container.innerHTML = '';

    const blankElement = document.querySelector('.model-blank');

    if (models.length === 0) {
        container.style.display = "none";
        if (blankElement) {
            blankElement.style.display = "flex";
        }
    } else {
        container.style.display = "flex";
        if (blankElement) {
            blankElement.style.display = "none";
        }
        models.forEach(model => {
            const item = document.createElement('div');
            item.className = 'model-list-item';
            item.innerHTML = `
                <div class="col">
                    <h2 class="text-14px reguler">${model.model_id}</h2>
                    <p class="mono-14px reguler">${model.model_type}</p>
                </div>
                <div class="col">
                    <div class="badge" data-state="${model.status === 'loaded' ? 'success' : model.status === 'unloaded' ? 'error' : ''}" data-text="${model.status === 'loaded' ? 'LOADED' : model.status === 'unloaded' ? 'UNLOADED' : ''}"></div>
                    <button class="btn-sm-icon btn-outline"><i class="ri-delete-bin-line"></i></button>
                </div>
            `;
            container.appendChild(item);
        });
    }

    Badge();
}

function ModelList(filter = "All") {
    if (!window.modelsData) {
        ShowModelLoading();
        fetch(getApiUrl('models'))
            .then(response => response.json())
            .then(data => {
                window.modelsData = data.models;
                let filteredModels;
                if (filter === "All") {
                    filteredModels = window.modelsData;
                } else {
                    filteredModels = window.modelsData.filter(model => model.model_type.toLowerCase() === filter.toLowerCase());
                }
                RenderModel(filteredModels);
            });
    } else {
        let filteredModels;
        if (filter === "All") {
            filteredModels = window.modelsData;
        } else {
            filteredModels = window.modelsData.filter(model => model.model_type.toLowerCase() === filter.toLowerCase());
        }
        RenderModel(filteredModels);
    }
}

function FilterModel() {
    const buttons = document.querySelectorAll('#FilterModel button');
    buttons.forEach(button => {
        button.addEventListener('click', () => {
            buttons.forEach(btn => btn.classList.remove('active'));
            button.classList.add('active');
            const filter = button.textContent.trim();
            ModelList(filter);
        });
    });
}

function SearchModel() {
    const searchInput = document.querySelector('#SearchModel');
    if (!searchInput) return;
    searchInput.addEventListener('input', () => {
        const value = searchInput.value.trim().toLowerCase();
        if (!window.modelsData) return;
        const filtered = window.modelsData.filter(model =>
            model.model_id.toLowerCase().includes(value)
        );
        RenderModel(filtered);
    });
}

function ShowModelLoading() {
    const container = document.querySelector('.model-list');
    if (!container) return;
    container.innerHTML = '';
    for (let i = 0; i < 4; i++) {
        const loadingItem = document.createElement('div');
        loadingItem.className = 'model-list-loading';
        container.appendChild(loadingItem);
    }
}

HeadInitiate();
Sidebar();
ModelList();
FilterModel();
SearchModel();
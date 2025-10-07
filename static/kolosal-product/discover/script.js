import { Badge } from "../component/badge.js";
import { HeadInitiate } from "../component/head.js";
import { Popup } from "../component/popup.js";
import { Sidebar } from "../component/sidebar.js";
import { getApiUrl } from "../config.js";

// Global state to track selected model and file
let selectedModelId = "";
let selectedFileName = "";

async function ModelList({ searchTerm = "", reset = false } = {}) {
    const container = document.querySelector(".model-list");
    if (!container) return;
    if (reset) { container.innerHTML = "";}

    function renderModels(data) {
        data.forEach(model => {
            const id = model.id || "Unknown Model";
            const task = model.pipeline_tag || "Unknown Task";
            const siblingsCount = model.siblings ? model.siblings.filter(file => file.rfilename.endsWith(".gguf")).length : 0;

            const item = document.createElement("div");
            item.className = "model-list-item";
            item.innerHTML = `
                <div class="model-name">
                    <h2 class="text-14px reguler">${id}</h2>
                    <p class="mono-12px reguler">${task}</p>
                </div>
                <div class="model-detail">
                    <i class="ri-import-line"></i>
                    <h3 class="text-12px reguler">${siblingsCount} files</h3>
                </div>
                <button class="btn-md btn-outline" data-popup="FileList">Add Model</button>
                `;

            item.querySelector("[data-popup]").addEventListener("click", async () => {
                const listContainer = document.querySelector(".popup-model-list");
                if (!listContainer) return;

                // Store the selected model ID
                selectedModelId = id;

                listContainer.innerHTML = "";
                SkeletonFile(listContainer, 5);

                if (model.siblings && model.siblings.length > 0) {
                    for (const file of model.siblings.filter(file => file.rfilename.endsWith(".gguf"))) {
                        const fileItem = document.createElement("div");
                        fileItem.className = "file-item";
                        fileItem.style.cursor = "pointer";

                        let sizeText = "";
                        try {
                            const head = await fetch(`https://huggingface.co/${id}/resolve/main/${file.rfilename}`, { method: "HEAD" });
                            const size = head.headers.get("content-length");
                            if (size) {
                                sizeText = (size / 1e9).toFixed(1) + " GB";
                            }
                        } catch (e) {
                            sizeText = "";
                        }

                        fileItem.innerHTML = `
                            <h2 class="text-14px reguler">${file.rfilename}</h2>
                            <h3 class="mono-14px reguler">${sizeText}</h3>
                        `;
                        
                        // Add click listener to select the file
                        fileItem.addEventListener("click", () => {
                            // Remove selection from other items
                            listContainer.querySelectorAll(".file-item").forEach(item => {
                                item.style.backgroundColor = "";
                                item.style.borderColor = "";
                            });
                            // Highlight selected item
                            fileItem.style.backgroundColor = "#f0f0f0";
                            fileItem.style.borderColor = "#007bff";
                            selectedFileName = file.rfilename;
                        });
                        
                        listContainer.appendChild(fileItem);
                        const skeleton = listContainer.querySelector(".file-item-loading");
                        if (skeleton) skeleton.remove();
                    }
                    const skeletons = listContainer.querySelectorAll(".file-item-loading");
                    skeletons.forEach(skeleton => skeleton.remove());
                } else {
                    const skeletons = listContainer.querySelectorAll(".file-item-loading");
                    skeletons.forEach(skeleton => skeleton.remove());
                    listContainer.innerHTML = `<p class="mono-12px reguler">No files available</p>`;
                }
            });
            container.appendChild(item);
        });
    }

    if (searchTerm === "") {
        let skip = 0;
        const batchSize = 32;
        let loading = false;
        let finished = false;

        async function fetchBatch() {
            if (loading || finished) return;
            loading = true;

            SkeletonModel(container, 8);

            if (skip === 0) {
                container.innerHTML = "";
            }

            const response = await fetch(
                `https://huggingface.co/api/models?filter=gguf&pipeline_tag=text-generation&sort=trendingScore&limit=${batchSize}&skip=${skip}&full=true`,
                {
                    method: "GET"
                }
            );
            const data = await response.json();

            const skeletons = container.querySelectorAll(".model-list-loading");
            skeletons.forEach(skeleton => skeleton.remove());

            renderModels(data);

            Popup();

            if (data.length < batchSize) {
                finished = true;
            }
            skip += batchSize;
            loading = false;
        }

        fetchBatch();

        const wrapper = document.querySelector(".wrapper");
        if (wrapper) {
            wrapper.addEventListener("scroll", () => {
                if (wrapper.scrollTop + wrapper.clientHeight >= wrapper.scrollHeight - 10) {
                    if (!loading && !finished) {
                        fetchBatch();
                    }
                }
            });
        }
    } else {
        container.innerHTML = "";
        SkeletonModel(container, 8);

        const response = await fetch(
            `https://huggingface.co/api/models?search=${encodeURIComponent(searchTerm)}&filter=gguf&pipeline_tag=text-generation&limit=32&full=true`,
            {
                method: "GET"
            }
        );
        const data = await response.json();
        const skeletons = container.querySelectorAll(".model-list-loading");
        skeletons.forEach(skeleton => skeleton.remove());
        if (!Array.isArray(data) || data.length === 0) {
            container.innerHTML = `<p class="mono-12px reguler" style="padding: 1rem;">No results found.</p>`;
            return;
        }

        renderModels(data, "Add Model");
        Popup();
    }
}

function SkeletonModel(container, count = 8) {
    for (let i = 0; i < count; i++) {
        const skeleton = document.createElement("div");
        skeleton.className = "model-list-loading";
        container.appendChild(skeleton);
    }
}

function SkeletonFile(container, count = 5) {
    for (let i = 0; i < count; i++) {
        const skeleton = document.createElement("div");
        skeleton.className = "file-item-loading";
        container.appendChild(skeleton);
    }
}

function SearchModel() {
    const input = document.getElementById("SearchModel");
    if (!input) return;
    input.addEventListener("input", function () {
        const searchTerm = input.value.trim();
        ModelList({ searchTerm, reset: true });
    });
}

function SetupLoadModelForm() {
    const form = document.getElementById("LoadModelForm");
    if (!form) return;

    // Pre-populate the form when Load Model popup opens
    const loadModelButtons = document.querySelectorAll('[data-popup="LoadModel"]');
    loadModelButtons.forEach(button => {
        button.addEventListener("click", () => {
            if (!selectedFileName) {
                alert("Please select a file first!");
                return;
            }
            
            // Pre-populate form fields
            const modelIdInput = form.querySelector('[name="model_id"]');
            const modelPathInput = form.querySelector('[name="model_path"]');
            
            if (modelIdInput && selectedModelId) {
                // Create a clean model ID from the Hugging Face model ID
                const cleanModelId = selectedModelId.replace(/\//g, '-');
                modelIdInput.value = cleanModelId;
            }
            
            if (modelPathInput && selectedFileName) {
                // The model path would be the downloaded file path
                // Users will need to adjust this to their actual download location
                modelPathInput.value = `models/${selectedFileName}`;
            }
        });
    });

    // Handle form submission
    form.addEventListener("submit", async (e) => {
        e.preventDefault();
        
        const formData = new FormData(form);
        
        // Build the request payload
        const payload = {
            model_id: formData.get("model_id"),
            model_path: formData.get("model_path"),
            model_type: formData.get("model_type"),
            load_immediately: formData.get("load_immediately") === "on",
            main_gpu_id: parseInt(formData.get("main_gpu_id")),
            inference_engine: formData.get("inference_engine") || undefined,
            loading_parameters: {
                n_ctx: parseInt(formData.get("n_ctx")),
                n_keep: parseInt(formData.get("n_keep")),
                use_mlock: formData.get("use_mlock") === "on",
                use_mmap: formData.get("use_mmap") === "on",
                cont_batching: formData.get("cont_batching") === "on",
                warmup: formData.get("warmup") === "on",
                n_parallel: parseInt(formData.get("n_parallel")),
                n_gpu_layers: parseInt(formData.get("n_gpu_layers")),
                split_mode: parseInt(formData.get("split_mode")),
                n_batch: parseInt(formData.get("n_batch")),
                n_ubatch: parseInt(formData.get("n_ubatch"))
            }
        };
        
        // Parse tensor_split if provided
        const tensorSplitValue = formData.get("tensor_split");
        if (tensorSplitValue && tensorSplitValue.trim()) {
            const splits = tensorSplitValue.split(",").map(s => parseFloat(s.trim())).filter(n => !isNaN(n));
            if (splits.length > 0) {
                payload.loading_parameters.tensor_split = splits;
            }
        }
        
        // Remove inference_engine if not provided
        if (!payload.inference_engine) {
            delete payload.inference_engine;
        }
        
        try {
            // Show loading state
            const submitButton = form.querySelector('button[type="submit"]');
            const originalText = submitButton.textContent;
            submitButton.textContent = "Loading Model...";
            submitButton.disabled = true;
            
            const response = await fetch(getApiUrl("models"), {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify(payload)
            });
            
            if (!response.ok) {
                const errorData = await response.json();
                throw new Error(errorData.error || `HTTP error! status: ${response.status}`);
            }
            
            const result = await response.json();
            console.log("Model loaded successfully:", result);
            
            // Show success message
            alert("Model loaded successfully!");
            
            // Reset form and close popup
            form.reset();
            document.querySelector('.popup[data-popup="LoadModel"] .ClosePopup').click();
            document.querySelector('.popup[data-popup="FileList"] .ClosePopup').click();
            
            // Refresh models if on models page
            if (window.modelsData) {
                window.modelsData = null; // Force refresh
            }
            
        } catch (error) {
            console.error("Error loading model:", error);
            alert(`Error loading model: ${error.message}`);
        } finally {
            // Restore button state
            const submitButton = form.querySelector('button[type="submit"]');
            submitButton.textContent = "Load Model";
            submitButton.disabled = false;
        }
    });
}

HeadInitiate();
Sidebar();
Badge();
ModelList();
SearchModel();
SetupLoadModelForm();
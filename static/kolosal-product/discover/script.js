import { Badge } from "../component/badge.js";
import { HeadInitiate } from "../component/head.js";
import { Popup } from "../component/popup.js";
import { Sidebar } from "../component/sidebar.js";

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

                listContainer.innerHTML = "";
                SkeletonFile(listContainer, 5);

                if (model.siblings && model.siblings.length > 0) {
                    for (const file of model.siblings.filter(file => file.rfilename.endsWith(".gguf"))) {
                        const fileItem = document.createElement("div");
                        fileItem.className = "file-item";

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

HeadInitiate();
Sidebar();
Badge();
ModelList();
SearchModel();
import { HeadInitiate } from "../../component/head.js";
import { InputFile } from "../../component/input.js";
import { Select } from "../../component/select.js";
import { Sidebar, SidebarAlt } from "../../component/sidebar.js";
import { getApiUrl } from "../../config.js";

function DocumentType() {
    const templates = [
        {
            label: "PDF",
            icon: "https://res.cloudinary.com/dh21tvktj/image/upload/v1758776679/doc-type-pdf_xfgtrj.svg",
            active: true,
        },
        {
            label: "XLX",
            icon: "https://res.cloudinary.com/dh21tvktj/image/upload/v1758777010/doc-type-xlx_aduyd7.svg",
        },
        {
            label: "PPTX",
            icon: "https://res.cloudinary.com/dh21tvktj/image/upload/v1758777010/doc-type-ppt_nfotvs.svg",
        },
        {
            label: "DOC",
            icon: "https://res.cloudinary.com/dh21tvktj/image/upload/v1758777010/doc-type-doc_rb2dre.svg",
        },
        {
            label: "HTML",
            icon: "https://res.cloudinary.com/dh21tvktj/image/upload/v1758777010/doc-type-html_sm63hj.svg",
        },
    ];

    document.querySelectorAll(".document-list").forEach((list) => {
        const items = templates
            .map(({ label, icon, active = false }) => `
                <div class="document-list-item${active ? " active" : ""}">
                    <img src="${icon}" alt="${label}">
                    <h2 class="text-12px medium">${label}</h2>
                </div>
            `)
            .join("");

        list.innerHTML = items;

        const itemNodes = Array.from(list.querySelectorAll(".document-list-item"));
        itemNodes.forEach((item) => {
            item.addEventListener("click", () => {
                itemNodes.forEach((node) => node.classList.remove("active"));
                item.classList.add("active");
            });
        });
    });
}

function fileToBase64(file) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => {
            const result = reader.result;
            const base64 = result.split(',')[1];
            resolve(base64);
        };
        reader.onerror = error => reject(error);
        reader.readAsDataURL(file);
    });
}

function SubmitFileForm() {
    const button = document.querySelector(".submit-file");
    if (!button) return;

    const chunkingForm = document.querySelector("#ChunkingForm");
    const similarityForm = document.querySelector("#SimilarityForm");

    if (chunkingForm && similarityForm) {
        const observer = new MutationObserver(() => {
            const selectedChunking = (chunkingForm.getAttribute("data-selected") || "");
            if (selectedChunking === "Semantic Chunking") {
                similarityForm.classList.remove("disable");
            } else {
                similarityForm.classList.add("disable");
            }
        });
        observer.observe(chunkingForm, { attributes: true, attributeFilter: ["data-selected"] });
    }

    button.addEventListener("click", async () => {
        const fileInput = document.querySelector('.input-file input[type="file"]');
        const file = fileInput?.files[0];
        const fileName = file?.name || null;
        const docType = document.querySelector(".document-list-item.active h2")?.textContent?.toLowerCase() || null;
        const parser = document.querySelector("#ParserForm")?.getAttribute("data-selected") || null;

        if (!file || !fileName || !parser || !docType) {
            return;
        }

        let endpoint;
        if (["Kolosal Parser", "MarkItDown"].includes(parser)) {
            endpoint = getApiUrl(`parse_${docType}`);
        } else if (parser === "Docling") {
            endpoint = getApiUrl('v1/convert/file');
        } else {
            return;
        }

        const base64Data = await fileToBase64(file);
        const parseResponse = await fetch(endpoint, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                docType,
                data: base64Data,
                fileName: fileName.trim()
            })
        });

        if (!parseResponse.ok) {
            return;
        }

        const contentPreview = document.querySelector(".content-preview");
        if (contentPreview) {
            contentPreview.style.display = "flex";
        }

        const parsedResult = await parseResponse.json();

        const previewBox = document.querySelector("#PreviewFile");
        if (previewBox) {
            const contentText = parsedResult.text || parsedResult.data || parsedResult.content || "";
            previewBox.innerHTML = `<p class="mono-12px reguler">${contentText}</p>`;
        }

        const text = parsedResult?.text || parsedResult?.data || parsedResult?.content || "";
        if (text) {
            const similarityInput = document.querySelector("#SimilarityForm");
            const similarity = similarityInput ? parseFloat(similarityInput.value) : 0.5;
            const docType = document.querySelector(".document-list-item.active h2")?.textContent?.toLowerCase() || null;

            const chunkingBody = {
                docType,
                text,
                model_name: "qwen3-embedding-4b",
                similarity
            };

            const chunkingResponse = await fetch(getApiUrl('chunking'), {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(chunkingBody)
            });

            if (!chunkingResponse.ok) return;

            const chunkingResult = await chunkingResponse.json();

            if (chunkingResult && chunkingResult.chunks) {
                const previewList = document.querySelector(".preview-list");
                if (previewList) {
                    previewList.innerHTML = "";
                    chunkingResult.chunks.forEach((chunk, i) => {
                        const item = document.createElement("div");
                        item.classList.add("preview-list-item");
                        item.innerHTML = `
                            <div class="title">
                                <h2 class="text-14px reguler">Chunk-${i + 1}</h2>
                                <button class="btn-sm-icon btn-secondary"><i class="ri-delete-bin-line"></i></button>
                            </div>
                            <div class="body">
                                <p class="mono-12px reguler">${chunk.text}</p>
                            </div>
                            <div class="metadata">
                                <h2 class="mono-12px reguler">Metadata:</h2>
                                <pre class="mono-12px reguler">${JSON.stringify({
                                    chunk_index: chunk.index,
                                    token_count: chunk.token_count
                                }, null, 4)}</pre>
                            </div>
                        `;
                        previewList.appendChild(item);
                    });

                    AddDocument(chunkingResult.chunks);
                }
            }
        }
    });
}

function AddDocument(documents) {
    const btn = document.querySelector(".add-document");
    if (!btn) return;
    btn.innerHTML = "Add to Document";

    const newBtn = btn.cloneNode(true);
    btn.parentNode.replaceChild(newBtn, btn);

    newBtn.addEventListener("click", async () => {
        const scoreInput = document.querySelector("#ScoreThreshold input");
        const similarityInput = document.querySelector("#SimilarityForm");
        const similarity = similarityInput ? parseFloat(similarityInput.value) : 0.5;

        const response = await fetch(getApiUrl('add_documents'), {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ documents, similarity })
        });

        if (!response.ok) return;
        location.reload();
    });
}

HeadInitiate();
Sidebar();
SidebarAlt();
Select();
InputFile();
DocumentType();
SubmitFileForm();
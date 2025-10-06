import { HeadInitiate } from "../../component/head.js";
import { Select } from "../../component/select.js";
import { Sidebar, SidebarAlt } from "../../component/sidebar.js";
import { getApiUrl } from "../../config.js";

async function DocumentList() {
    const container = document.querySelector(".content-list");
    if (!container) return;
    container.innerHTML = Array.from({ length: 4 }).map(() => `
        <div class="content-list-loading"></div>
    `).join("");

    const listRes = await fetch(getApiUrl('list_documents'));
    if (!listRes.ok) return;
    const listData = await listRes.json();
    const countEl = document.getElementById("CountDocument");
    if (countEl) countEl.textContent = listData.total_count;
    const documentIds = listData.document_ids || [];

    const infoRes = await fetch(getApiUrl('info_documents'), {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ids: documentIds })
    });
    if (!infoRes.ok) return;
    const infoData = await infoRes.json();

    const itemsHTML = (infoData.documents || []).map((doc, index) => `
        <div class="content-list-item">
            <div class="title">
                <button class="btn-sm-icon btn-secondary OpenBody"><i class="ri-arrow-down-s-line"></i></button>
                <h2 class="text-14px reguler">Document-${index + 1}</h2>
                <p class="mono-14px reguler">${doc.id}</p>
                <button class="btn-sm-icon btn-secondary"><i class="ri-delete-bin-line"></i></button>
            </div>
            <div class="body">
                <h2 class="mono-12px reguler">Content:</h2>
                <h3 class="text-12px reguler">${doc.text}</h3>
            </div>
            <div class="body">
                <h2 class="mono-12px reguler">Metadata:</h2>
                <pre class="mono-12px reguler">${JSON.stringify(doc.metadata, null, 4)}</pre>
            </div>
        </div>
    `).join("");

    container.innerHTML = itemsHTML;
    AccordionItem();
    SortDocument();
}

function AccordionItem() {
    const buttons = document.querySelectorAll(".OpenBody");
    buttons.forEach(button => {
        button.addEventListener("click", () => {
            const item = button.closest(".content-list-item");
            if (!item) return;
            const bodies = item.querySelectorAll(".body");
            const isActive = Array.from(bodies).every(body => body.classList.contains("active"));

            if (isActive) {
                bodies.forEach(body => {
                    body.classList.remove("active");
                });
            } else {
                const activeBodies = document.querySelectorAll(".content-list-item .body.active");
                activeBodies.forEach(body => {
                    body.classList.remove("active");
                });
                bodies.forEach(body => {
                    body.classList.add("active");
                });
            }
        });
    });
}

function SortDocument() {
    const selectEl = document.getElementById("SortingCollection");
    if (!selectEl) return;
    const container = document.querySelector(".content-list");
    if (!container) return;
    if (!container._originalOrder) {
        container._originalOrder = Array.from(container.querySelectorAll(".content-list-item"));
    }

    const applySort = () => {
        const sortType = selectEl.getAttribute("data-selected");
        let items;
        if (sortType === "Latest") {
            items = [...container._originalOrder].reverse();
        } else {
            items = [...container._originalOrder];
        }
        container.innerHTML = "";
        items.forEach(item => container.appendChild(item));
    };

    applySort();

    const observer = new MutationObserver(() => {
        applySort();
    });
    observer.observe(selectEl, {
        attributes: true,
        attributeFilter: ["data-selected"]
    });
}

HeadInitiate();
Sidebar();
SidebarAlt();
Select();
DocumentList();
import { Badge } from "../../component/badge.js";
import { HeadInitiate } from "../../component/head.js";
import { Sidebar, SidebarAlt } from "../../component/sidebar.js";
import { getApiUrl } from "../../config.js";

function AccordionItem() {
    const buttons = document.querySelectorAll(".OpenBody");
    buttons.forEach(button => {
        button.addEventListener("click", () => {
            const item = button.closest(".item");
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

function SearchState(items, startTime) {
    const titleFirst = document.querySelector('.content-result-title h2:first-child');
    const titleLast = document.querySelector('.content-result-title h2:last-child');
    const elapsed = ((performance.now() - startTime) / 1000).toFixed(2);
    if (titleFirst) {
        titleFirst.textContent = `Result: ${items.length} items`;
    }
    if (titleLast) {
        titleLast.textContent = `${elapsed}s`;
    }
    const blank = document.querySelector('.content-result-blank');
    const list = document.querySelector('.content-result-list');
    if (items.length > 0) {
        if (blank) blank.style.display = 'none';
        if (list) list.style.display = 'flex';
    } else {
        if (blank) blank.style.display = 'flex';
        if (list) list.style.display = 'none';
    }
}

function SearchData() {
    document.getElementById('FormAction').addEventListener('click', async () => {
        const blank = document.querySelector('.content-result-blank');
        const listContent = document.querySelector('.content-result-list');
        if (blank) blank.style.display = 'none';
        if (listContent) listContent.style.display = 'flex';

        listContent.innerHTML = '';
        for (let i = 0; i < 3; i++) {
            const placeholder = document.createElement('div');
            placeholder.className = 'item-loading';
            listContent.appendChild(placeholder);
        }

        const query = document.getElementById('FormQuery').value.trim();
        const limit = parseInt(document.getElementById('FormLimit').value, 10);
        const scoreThreshold = parseFloat(document.getElementById('FormThreshold').value);
        const startTime = performance.now();

        const response = await fetch(getApiUrl('retrieve'), {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                query,
                limit,
                score_threshold: scoreThreshold
            })
        });

        const data = await response.json();
        listContent.innerHTML = '';

        const items = Array.isArray(data.documents) ? data.documents : [];
        SearchState(items, startTime);
        if (items.length > 0) {
            items.forEach((item) => {
                const div = document.createElement('div');
                div.className = 'item';
                div.innerHTML = `
                    <div class="title">
                        <button class="btn-sm-icon btn-secondary OpenBody"><i class="ri-arrow-down-s-line"></i></button>
                        <h2 class="mono-14px reguler">${item.id}</h2>
                        <div class="badge" data-text="${(item.score * 100).toFixed(2)}%"></div>
                    </div>
                    <div class="body">
                        <h2 class="mono-12px reguler">Content:</h2>
                        <h3 class="text-12px reguler">${item.text}</h3>
                    </div>
                    <div class="body">
                        <h2 class="mono-12px reguler">Metadata:</h2>
                        <pre class="mono-12px reguler">${JSON.stringify(item.metadata, null, 2)}</pre>
                    </div>
                `;
                listContent.appendChild(div);
            });

            AccordionItem();
            Badge();
        }
    });
}

HeadInitiate();
Sidebar();
SidebarAlt();
SearchData();
export function Select() {
    const selects = document.querySelectorAll(".select");
    if (!selects.length) return;

    selects.forEach((select) => {
        const trigger = select.querySelector(".select-trigger");
        const list = select.querySelector(".select-list");

        trigger.addEventListener("click", (e) => {
            e.stopPropagation();
            list.classList.toggle("active");
            trigger.classList.toggle("active");
            const icon = trigger.querySelector(".select-icon");
            if (icon) icon.classList.toggle("rotate");
        });

        const items = select.querySelectorAll(".select-list-item");
        items.forEach((item) => {
            item.addEventListener("click", (e) => {
                e.stopPropagation();
                const itemText = item.querySelector("h2")?.textContent;
                if (itemText) {
                    const triggerText = trigger.querySelector("h2");
                    if (triggerText) triggerText.textContent = itemText;
                    select.setAttribute("data-selected", itemText);
                }
                const itemImg = item.querySelector("img");
                const itemIcon = item.querySelector("i");
                if (itemImg) {
                    trigger.querySelectorAll("i:not(.select-icon)").forEach(i => i.remove());
                    let triggerImg = trigger.querySelector("img");
                    if (!triggerImg) {
                        const triggerText = trigger.querySelector("h2");
                        triggerImg = document.createElement("img");
                        if (triggerText) {
                            trigger.insertBefore(triggerImg, triggerText);
                        } else {
                            trigger.appendChild(triggerImg);
                        }
                    }
                    triggerImg.src = itemImg.src;
                    triggerImg.alt = itemImg.alt;
                } else if (itemIcon) {
                    trigger.querySelectorAll("img").forEach(img => img.remove());
                    let triggerIcon = trigger.querySelector("i:not(.select-icon)");
                    if (!triggerIcon) {
                        const triggerText = trigger.querySelector("h2");
                        triggerIcon = document.createElement("i");
                        if (triggerText) {
                            trigger.insertBefore(triggerIcon, triggerText);
                        } else {
                            trigger.appendChild(triggerIcon);
                        }
                    }
                    triggerIcon.className = itemIcon.className;
                }
                list.classList.remove("active");
                trigger.classList.remove("active");
                const arrowIcon = trigger.querySelector(".select-icon");
                if (arrowIcon) arrowIcon.classList.remove("rotate");
            });
        });

        document.addEventListener("click", (e) => {
            if (!select.contains(e.target)) {
                list.classList.remove("active");
                trigger.classList.remove("active");
                const icon = trigger.querySelector(".select-icon");
                if (icon) icon.classList.remove("rotate");
            }
        });
    });
}
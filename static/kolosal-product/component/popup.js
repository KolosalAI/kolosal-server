export function Popup() {
    const triggers = document.querySelectorAll("[data-popup]");
    triggers.forEach(trigger => {
        trigger.addEventListener("click", () => {
            const popupId = trigger.getAttribute("data-popup");
            const popup = document.querySelector(`.popup[data-popup="${popupId}"]`);
            if (popup) {
                popup.classList.toggle("active");
            }
        });
    });

    const closeButtons = document.querySelectorAll(".ClosePopup");
    closeButtons.forEach(btn => {
        btn.addEventListener("click", () => {
            const popup = btn.closest(".popup");
            if (popup) {
                popup.classList.remove("active");
            }
        });
    });

    const popupContents = document.querySelectorAll(".popup .popup-content");
    popupContents.forEach(content => {
        content.addEventListener("click", (e) => {
            e.stopPropagation();
        });
    });
}
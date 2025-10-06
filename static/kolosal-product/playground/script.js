import { HeadInitiate } from "../component/head.js";
import { Select } from "../component/select.js";
import { Sidebar } from "../component/sidebar.js";

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

HeadInitiate();
Sidebar();
Select();
ToggleOption();
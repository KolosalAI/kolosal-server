export function Badge() {
    const badges = document.querySelectorAll('.badge');

    badges.forEach(badge => {
        const state = badge.getAttribute('data-state');
        const text = badge.getAttribute('data-text');

        let color = "#1C1C1D";
        if (state === "success") {
            color = "#05E344";
        } else if (state === "error") {
            color = "#FF2723";
        } else if (state === "warning") {
            color = "#FFFF04";
        }

        badge.innerHTML = `
            ${state ? `<div class="badge-indicator" style="background-color:${color};"></div>` : ''}
            <h2 class="mono-12px medium">${text || ""}</h2>
        `;
    });
}
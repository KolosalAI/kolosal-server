export function HeadInitiate() {
    const iconSet = document.createElement("link");
    iconSet.rel = "stylesheet";
    iconSet.href = "https://cdn.jsdelivr.net/npm/remixicon@4.5.0/fonts/remixicon.css";
    document.head.appendChild(iconSet);

    // Use .ico file which supports multiple sizes
    const favicon = document.createElement("link");
    favicon.rel = "icon";
    favicon.type = "image/x-icon";
    favicon.href = "/favicon.ico";
    document.head.appendChild(favicon);

    // Also add shortcut icon for better browser compatibility
    const shortcutIcon = document.createElement("link");
    shortcutIcon.rel = "shortcut icon";
    shortcutIcon.type = "image/x-icon";
    shortcutIcon.href = "/favicon.ico";
    document.head.appendChild(shortcutIcon);
}
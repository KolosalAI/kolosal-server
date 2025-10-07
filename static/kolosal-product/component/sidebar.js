export function Sidebar() {
        const sidebar = document.querySelector('.sidebar');
    if (!sidebar) return;
    sidebar.innerHTML = `
            <div class="sidebar-logo">
                <a href="/"><img src="https://res.cloudinary.com/dh21tvktj/image/upload/v1758619535/kolosal-logo_oqierj.svg" alt="Kolosal logo"></a>
            </div>
            <div class="sidebar-menu">
                <a class="sidebar-menu-item" href="/">
                    <i class="ri-chat-ai-line"></i>
                    <div class="sidebar-menu-caption">
                        <p class="text-12px reguler">Playground</p>
                    </div>
                </a>
                <a class="sidebar-menu-item" href="/retrieve/index.html">
                    <i class="ri-dashboard-line"></i>
                    <div class="sidebar-menu-caption">
                        <p class="text-12px reguler">Retrieve</p>
                    </div>
                </a>
                <a class="sidebar-menu-item" href="/model/index.html">
                    <i class="ri-instance-line"></i>
                    <div class="sidebar-menu-caption">
                        <p class="text-12px reguler">Model</p>
                    </div>
                </a>
                <a class="sidebar-menu-item" href="/discover/index.html">
                    <i class="ri-shapes-line"></i>
                    <div class="sidebar-menu-caption">
                        <p class="text-12px reguler">Discover</p>
                    </div>
                </a>
            </div>
            <div class="sidebar-nav">
                <a class="sidebar-nav-item" href="#"><i class="ri-settings-3-line"></i></a>
            </div>
        `;

    const menuItems = sidebar.querySelectorAll('.sidebar-menu-item');
    menuItems.forEach(item => {
        item.addEventListener('click', () => {
            menuItems.forEach(el => el.classList.remove('active'));
            item.classList.add('active');
        });
    });

    const currentPath = window.location.pathname;
    menuItems.forEach(item => {
        const href = item.getAttribute('href');
        if (href !== "#") {
            if (href.includes("/retrieve/index.html") && currentPath.startsWith("/retrieve")) {
                menuItems.forEach(el => el.classList.remove('active'));
                item.classList.add('active');
            } else if (currentPath.endsWith(href) || currentPath === href) {
                menuItems.forEach(el => el.classList.remove('active'));
                item.classList.add('active');
            }
        }
    });
}

export function SidebarAlt() {
    const sidebarAlt = document.querySelector('.sidebar-alt');
    if (!sidebarAlt) return;
    sidebarAlt.innerHTML = `
        <div class="sidebar-alt-title">
            <h1 class="mono-14px medium">MENUS</h1>
        </div>
        <div class="sidebar-alt-list">
            <a class="list-item active" href="/retrieve/index.html">
                <i class="ri-signal-tower-line"></i>
                <h2 class="text-14px reguler">Status</h2>
            </a>
            <a class="list-item" href="/retrieve/upload.html">
                <i class="ri-export-line"></i>
                <h2 class="text-14px reguler">Upload</h2>
            </a>
            <a class="list-item" href="/retrieve/search.html">
                <i class="ri-menu-search-line"></i>
                <h2 class="text-14px reguler">Search</h2>
            </a>
            <a class="list-item" href="/retrieve/collection.html">
                <i class="ri-archive-line"></i>
                <h2 class="text-14px reguler">Collection</h2>
            </a>
        </div>
    `;
    const listItems = sidebarAlt.querySelectorAll('.list-item');
    listItems.forEach(item => item.classList.remove('active'));
    const currentPath = window.location.pathname;
    let matched = false;
    listItems.forEach(item => {
        const href = item.getAttribute('href');
        if (!href) return;
        if (currentPath === href || currentPath.endsWith(href)) {
            item.classList.add('active');
            matched = true;
        }
    });
    
    if (!matched && currentPath.startsWith('/retrieve')) {
        listItems.forEach(item => {
            const href = item.getAttribute('href');
            if (href === '/retrieve/index.html') {
                item.classList.add('active');
            }
        });
    }
}
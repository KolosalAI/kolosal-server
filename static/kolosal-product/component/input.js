export function InputFile() {
    const container = document.querySelector('.input-file');
    const input = container.querySelector('input[type="file"]');
    const defaultHTML = container.innerHTML;

    container.addEventListener('click', (e) => {
        const cancelBtn = e.target.closest('.cancel-btn');
        if (cancelBtn) return;
        if (e.target === input) return;
        input.click();
    });

    input.addEventListener('change', () => {
        if (input.files.length > 0) {
            const file = input.files[0];
            container.classList.add('file-selected');
            
            container.innerHTML = `
                <div class="file-info">
                    <h3 class="text-12px reguler">${file.name}</h3>
                    <button class="btn-sm-icon btn-outline cancel-btn"><i class="ri-close-line"></i></button>
                </div>
            `;
            container.appendChild(input);
            input.style.display = "none";

            const cancelBtn = container.querySelector('.cancel-btn');
            cancelBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                input.value = '';
                container.classList.remove('file-selected');
                container.innerHTML = defaultHTML;
                InputFile();
            });
        }
    });
}
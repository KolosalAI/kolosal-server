import { HeadInitiate } from "../component/head.js";

function StepForm() {
   const forms = document.querySelectorAll(".form");
    let currentStep = 0;

    forms.forEach((f, i) => {
        f.style.display = (i === currentStep) ? "flex" : "none";
    });

    forms.forEach((form, i) => {
        const nextBtn = form.querySelector(".NextForm");
        const submitBtn = form.querySelector(".SubmitForm");

        if (nextBtn) {
            nextBtn.addEventListener("click", () => {
                if (i < forms.length - 1) {
                    forms[i].style.display = "none";
                    forms[i + 1].style.display = "flex";
                    currentStep = i + 1;
                }
            });
        }

        if (submitBtn) {
            submitBtn.addEventListener("click", () => {
                alert("Register berhasil!");
            });
        }
    }); 
}

HeadInitiate();
StepForm();
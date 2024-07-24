const endpoint =
  "https://emailapi.endpoints.isentropic-card-423523-k4.cloud.goog";

function getCurrentSecondOfDay() {
  const now = new Date();
  const hours = now.getHours();
  const minutes = now.getMinutes();
  const seconds = now.getSeconds();

  const totalSeconds = hours * 3600 + minutes * 60 + seconds;
  return totalSeconds;
}

// Switch element //
let switch_checkbox = document.createElement("input");
switch_checkbox.setAttribute("type", "checkbox");

switch_checkbox.addEventListener("change", () => {
  const compose_body = document.querySelector(
    ".Am.aiL.Al.editable.LW-avf.tS-tW"
  );

  const send_button_div = document.querySelector(".J-J5-Ji.btA");
  if (switch_checkbox.checked) {
    send_button_div.addEventListener(
      "click",
      (event) => {
        const tracker = document.createElement("img");
        const subject = document.querySelector(
          "input[name='subjectbox']"
        ).value;

        const one_has_fromarialabel = document.querySelectorAll(".gb_d");

        let from_arialabel = null;

        one_has_fromarialabel.forEach((element) => {
          if (element.hasAttribute("aria-label")) {
            from_arialabel = element.getAttribute("aria-label");
          }
        });

        const from = from_arialabel.match(/\(([^)]+)\)/)[1];
        const to_divs = document.querySelectorAll(".oL.aDm.az9");
        let to = "";
        to_divs.forEach((div, index) => {
          let email = div.querySelector("span").getAttribute("email");
          if (to !== "") {
            to += `;${email}`;
          } else {
            to += email;
          }
        });

        tracker.setAttribute(
          "src",
          `${endpoint}/signature.gif?f=${from}&t=${to}&s=${subject}&n=${getCurrentSecondOfDay()}`
        );
        tracker.setAttribute("alt", "");
        tracker.setAttribute("width", "0");
        tracker.setAttribute("height", "0");
        tracker.style.overflow = "hidden";

        compose_body.appendChild(tracker);

        console.log("tracker added");

        switch_checkbox.checked = false;
      },
      true
    );
  }
});

let switch_slider = document.createElement("span");
switch_slider.classList.add("slider");
switch_slider.classList.add("round");

let switch_element = document.createElement("label");
switch_element.setAttribute("id", "switch_element_id");
switch_element.classList.add("switch");

switch_element.appendChild(switch_checkbox);
switch_element.appendChild(switch_slider);

// Monitor the window for compose box //
const body_observer = new MutationObserver((mutations) => {
  let compose_footer = document.getElementsByClassName("btC")[0];
  if (compose_footer && !document.getElementById("switch_element_id")) {
    compose_footer.insertBefore(switch_element, compose_footer.children[1]);
  }
});

body_observer.observe(document.body, { childList: true, subtree: true });

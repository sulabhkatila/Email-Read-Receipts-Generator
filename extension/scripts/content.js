
// Switch element

let switch_checkbox = document.createElement('input');
switch_checkbox.setAttribute('type', 'checkbox');

let switch_slider = document.createElement('span');
switch_slider.classList.add('slider');
switch_slider.classList.add('round');

let switch_element = document.createElement('label');
switch_element.setAttribute('id', 'switch_element_id');
switch_element.classList.add('switch');

// Append checkbox and slider to the label
switch_element.appendChild(switch_checkbox);
switch_element.appendChild(switch_slider);


const body_observer = new MutationObserver((mutations) => {
    let compose_footer = document.getElementsByClassName('btC')[0];
    if (compose_footer && !(document.getElementById('switch_element_id'))) {
        compose_footer.insertBefore(switch_element, compose_footer.children[1]);
    }
})

body_observer.observe(document.body, { childList: true, subtree: true });


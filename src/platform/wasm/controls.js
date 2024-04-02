const buttonNameToId = new Map();
buttonNameToId.set('a', 0);
buttonNameToId.set('b', 1);
buttonNameToId.set('select', 2);
buttonNameToId.set('start', 3);
buttonNameToId.set('right', 4);
buttonNameToId.set('left', 5);
buttonNameToId.set('up', 6);
buttonNameToId.set('down', 7);
buttonNameToId.set('r', 8);
buttonNameToId.set('l', 9);
const buttonIdToName = new Map();
for (const [key, value] of buttonNameToId) {
  buttonIdToName.set(value, key);
}

// Use setTimeout with 16ms delays for 60fps.
// Ideally this would be 16.6666, but this has to be an integer...
const normalLoopTiming = 16;
const fastLoopTiming = 8;
const infinityLoopTiming = 1;
const slowLoopTiming = 64;

export default class MgbaControls extends HTMLElement {
  connectedCallback() {
    // 0 is normalLoopTiming aka 1x
    // 1 is fastLoopTiming aka 2x
    // 2 is infinityLoopTiming
    // 3 is slowLoopTiming aka 0.25x
    this.loopTiming = 0;

    this.addButtons();
  }

  addButtons() {
    const buttonContainer = document.createElement('div');
    buttonContainer.classList.add('button-container');
    this.appendChild(buttonContainer);

    this.addCanvasContainer(buttonContainer);
    this.addShoulderRow(buttonContainer);
    this.addControlsRow(buttonContainer);
    this.addMenuRow(buttonContainer);

    const A = this.querySelector('.A');
    const B = this.querySelector('.B');
    const L = this.querySelector('.L');
    const R = this.querySelector('.R');
    const start = this.querySelector('.start');
    const select = this.querySelector('.select');

    [[A, 'A'], [B, 'B'], [L, 'L'], [R, 'R'], [start, 'start'], [select, 'select']].forEach(([element, buttonName]) => {
      ['mousedown', 'touchstart'].forEach(eventName => {
        element.addEventListener(eventName, () => {
          this.buttonPress(buttonName);
          element.classList.add('pressed');
        });
      });
      ['mouseup', 'touchend', 'touchcancel'].forEach(eventName => {
        element.addEventListener(eventName, () => {
          this.buttonUnpress(buttonName);
          element.classList.remove('pressed');
        });
      });
    });

    const dpad = this.querySelector('.dpad');
    const clearDpad = () => {
      for (const name of ['Up', 'Down', 'Left', 'Right'])
        this.buttonUnpress(name);
    };
    const updateDpad = event => {
      const offsetLeft = event.target.offsetLeft - dpad.offsetLeft;
      const offsetTop = event.target.offsetTop - dpad.offsetTop;

      const x = (offsetLeft + event.offsetX) / dpad.offsetWidth;
      const y = (offsetTop + event.offsetY) / dpad.offsetHeight;
      const threshold = 1 / 3;
      const pressed = {
        Left: false,
        Right: false,
        Up: false,
        Down: false,
      }
      if (x < threshold) {
        pressed.Left = true;
      } else if (x > 1 - threshold) {
        pressed.Right = true;
      }
      if (y < threshold) {
        pressed.Up = true;
      } else if (y > 1 - threshold) {
        pressed.Down = true;
      }
      for (const name of ['Left', 'Right', 'Up', 'Down']) {
        if (pressed[name]) {
          this.buttonPress(name);
        } else {
          this.buttonUnpress(name);
        }
      }
    };

    dpad.onpointerdown = event => updateDpad(event);
    dpad.onpointermove = event => {
      if (!event.buttons && event.pointerType === 'mouse')
        return;
      updateDpad(event);
    };
    dpad.onpointerup = () => clearDpad();
    dpad.onpointerleave = () => clearDpad();
  }

  buttonPress(name) {
    window.Module._buttonPress(buttonNameToId.get(name.toLowerCase()));
  }

  buttonUnpress(name) {
    window.Module._buttonUnpress(buttonNameToId.get(name.toLowerCase()));
  }

  addShoulderRow(container) {
    const shoulderRow = document.createElement('div');
    container.appendChild(shoulderRow);
    shoulderRow.classList.add('shoulder-row');

    const L = document.createElement('div');
    L.classList.add('L');
    L.classList.add('button');
    shoulderRow.appendChild(L);

    const R = document.createElement('div');
    R.classList.add('R');
    R.classList.add('button');
    shoulderRow.appendChild(R);
  }

  addControlsRow(container) {
    const controlsRow = document.createElement('div');
    container.appendChild(controlsRow);
    controlsRow.classList.add('controls-row');

    const dpad = document.createElement('div');
    dpad.classList.add('dpad');
    controlsRow.appendChild(dpad);
    for (let i = 0; i < 9; i++) {
      const div = document.createElement('div');
      if (i == 0) {
        div.classList.add('up-left');
      } else if (i == 1) {
        div.classList.add('up');
      } else if (i == 2) {
        div.classList.add('up-right');
      } else if (i == 3) {
        div.classList.add('left');
      } else if (i == 4) {
        div.classList.add('center');
      } else if (i == 5) {
        div.classList.add('right');
      } else if (i == 6) {
        div.classList.add('down-left');
      } else if (i == 7) {
        div.classList.add('down');
      } else if (i == 8) {
        div.classList.add('down-right');
      }
      dpad.appendChild(div);
    }

    const abContainer = document.createElement('div');
    abContainer.classList.add('ab-container');
    controlsRow.appendChild(abContainer);

    const placeholderOne = document.createElement('div');
    placeholderOne.classList.add('ab-placeholder');
    abContainer.appendChild(placeholderOne);

    const a = document.createElement('div');
    a.classList.add('A');
    a.classList.add('button');
    abContainer.appendChild(a);

    const b = document.createElement('div');
    b.classList.add('B');
    b.classList.add('button');
    abContainer.appendChild(b);

    const placeholderTwo = document.createElement('div');
    placeholderTwo.classList.add('ab-placeholder');
    abContainer.appendChild(placeholderTwo);
  }

  addMenuRow(container) {
    const menuRow = document.createElement('div');
    container.appendChild(menuRow);
    menuRow.classList.add('menu-row');

    const menuSpeedContainer = document.createElement('div');
    menuSpeedContainer.classList.add('menu-speed-container');
    menuRow.appendChild(menuSpeedContainer);

    const menu = document.createElement('div');
    menu.classList.add('menu');
    menu.classList.add('button');
    menuSpeedContainer.appendChild(menu);
    menu.onclick = () => {
      document.querySelector('mgba-game').appendChild(document.createElement('mgba-game-menu'));
    };

    const speed = document.createElement('div');
    speed.classList.add('speed');
    speed.classList.add('button');
    speed.textContent = '1x';
    menuSpeedContainer.appendChild(speed);
    speed.onclick = () => {
      if (this.loopTiming == 0) {
        this.loopTiming = 1;
        speed.textContent = '2x';
        // TODO consider using EM_TIMING_RAF instead of EM_TIMING_SETTIMEOUT:
        // https://emscripten.org/docs/api_reference/emscripten.h.html#c.emscripten_set_main_loop_timing
        window.Module._setMainLoopTiming(0, fastLoopTiming);
      } else if (this.loopTiming == 1) {
        this.loopTiming = 2;
        speed.textContent = '♾️';
        window.Module._setMainLoopTiming(0, infinityLoopTiming);
      } else if (this.loopTiming == 2) {
        this.loopTiming = 3;
        speed.textContent = '0.25x';
        window.Module._setMainLoopTiming(0, slowLoopTiming);
      } else if (this.loopTiming == 3) {
        this.loopTiming = 0;
        speed.textContent = '1x';
        window.Module._setMainLoopTiming(0, normalLoopTiming);
      } else {
        console.error('unrecognized loopTiming: ' + this.loopTiming);
      }
    };

    const selectStartContainer = document.createElement('div');
    selectStartContainer.classList.add('select-start-container');
    menuRow.appendChild(selectStartContainer);

    const select = document.createElement('div');
    select.classList.add('select');
    select.classList.add('button');
    selectStartContainer.appendChild(select);

    const start = document.createElement('div');
    start.classList.add('start');
    start.classList.add('button');
    selectStartContainer.appendChild(start);

    menuRow.appendChild(document.createElement('div'));
  }

  addCanvasContainer(container) {

    const canvasContainer = document.createElement('div');
    canvasContainer.id = 'canvas-container';
    container.appendChild(canvasContainer);

    canvasContainer.appendChild(document.createElement('div'));
    canvasContainer.appendChild(document.getElementById('canvas'));
    canvasContainer.appendChild(document.createElement('div'));
  }

  disconnectedCallback() {
    document.body.appendChild(document.getElementById('canvas'));
  }
}

customElements.define('mgba-controls', MgbaControls);

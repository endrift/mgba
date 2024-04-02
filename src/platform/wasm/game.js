import * as FileLoader from './fileloader.js';
import MgbaGameLoader from './gamemenu.js';
import MgbaControls from './controls.js';
import MgbaInit from './init.js';

export default class MgbaGame extends HTMLElement {
  async connectedCallback() {
    document.body.classList.add('in-game');

    this.canvas = document.getElementById('canvas');

    // TODO this is hacky but loading the game again always breaks the c program
    if (!await MgbaInit.initMgba()) {
      const div = document.createElement('div');
      div.textContent = 'mGBA initialization failed!';
      this.appendChild(div);
      return;
    }

    this.controls = document.createElement('mgba-controls');
    this.appendChild(this.controls);

    this.canvas.setAttribute('width', '240');
    this.canvas.setAttribute('height', '240');
    this.canvas.style = 'cursor: default;';

    this.placeholder = document.createElement('div');
    this.appendChild(this.placeholder);
    this.placeholder.classList.add('placeholder');
    const placeholderTitle = document.createElement('h1');
    placeholderTitle.textContent = 'Loading...';
    this.placeholder.appendChild(placeholderTitle);

    window.Module.canvas = this.canvas;
    // TODO window.Module._setMainLoopTiming(0, MgbaGame.mainLoopTiming);
    this.placeholder.remove();
    this.canvas.classList.remove('disabled');

    if (!this.name)
      throw new Error('this.namenot defined! this: ', this);

    const romFilepath = `/data/games/${this.name}`;
    FileLoader.loadGame(romFilepath);

    // volume
    try {
      const volumeSetting = localStorage.getItem('volume');
      if (volumeSetting)
        window.Module._setVolume(volumeSetting);
    } catch (error) {
      console.error('setting volume threw an error! ', error);
    }
    
    const autosaveSlot = 0;

    // load save state if we have it
    let filepath = this.name;
    filepath = filepath.replace(/\.[^/.]+$/, ""); // remove file extension
    filepath = `/data/states/${filepath}.ss${autosaveSlot}`;
    try {
      await FileLoader.writefs();
      if (window.Module.FS.stat(filepath) && window.confirm('Load save state?')) {
        window.Module._loadState(autosaveSlot);
      }
    } catch (e) {
      // FS.stat() will throw if the file doesn't exist.
    }

    // auto save state every 2 seconds
    // TODO tweak this interval
    // TODO make this a setting in case people dont like autosaves
    const autosaveMs = 2000;
    const scheduleAutosave = () => {
      this.timeout = setTimeout(async () => {
        window.Module._saveState(autosaveSlot);
        await FileLoader.writefs();
        scheduleAutosave();
      }, autosaveMs);
    };
    scheduleAutosave();
  }

  disconnectedCallback() {
    document.body.classList.remove('in-game');
    if (this.timeout) {
      clearTimeout(this.timeout);
    }
    this.canvas.classList.add('disabled');
  }

  toggleTouchControls() {
    if (this.controls) {
      document.body.appendChild(document.getElementById('canvas'));
      this.controls.remove();
      this.controls = null;
    } else {
      this.controls = document.createElement('mgba-controls');
      this.appendChild(this.controls);
    }
  }
}

customElements.define('mgba-game', MgbaGame);

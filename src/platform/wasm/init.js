import MgbaMenu from './menu.js';
import * as FileLoader from './fileloader.js';

export default class MgbaInit extends HTMLElement {
  static async initMgba() {
    try {
      if (window.Module && window.Module._quitMgba) {
        try {
          window.Module._quitMgba();
        } catch (error) {
          // Qutting the program throws an exception i think
        }
        window.Module = {};
      }
      if (!window.Module)
        window.Module = {};

      window.Module.canvas = document.getElementById('canvas');

      try {
        await mGBA(window.Module);
      } catch (error) {
        console.error('mGBA() failed! ', error);
        return false;
      }

      // set up filesystem, this was moved from main.c
      window.Module.FS.mkdir('/data');
      window.Module.FS.mount(window.Module.FS.filesystems.IDBFS, {}, '/data');
      await FileLoader.readfs();
      // When we read from indexedb, these directories may or may not exist.
      // If we mkdir and they already exist they throw, so just catch all of them.
      try {
        window.Module.FS.mkdir('/data/saves');
      } catch (e) {}
      try {
        window.Module.FS.mkdir('/data/states');
      } catch (e) {}
      try {
        window.Module.FS.mkdir('/data/games');
      } catch (e) {}

      return true;
    } catch (error) {
      console.error('initMgba caught an error: ', error);
      return false;
    }
  }

  async connectedCallback() {
    const loading = document.createElement('h2');
    loading.textContent = 'Loading mGBA...';
    this.appendChild(loading);

    const canvas = document.createElement('canvas');
    canvas.id = 'canvas';
    canvas.classList.add('disabled');
    document.body.appendChild(canvas);

    if (!await MgbaInit.initMgba()) {
      loading.textContent = 'mGBA initialization failed!';
    }

    this.remove();
    document.body.appendChild(document.createElement('mgba-menu'));
  }
}

customElements.define('mgba-init', MgbaInit);

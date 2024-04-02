import MgbaSettingsDialog from './settings.js';
import MgbaGame from './game.js';
import * as FileLoader from './fileloader.js';

export default class MgbaGameMenu extends HTMLElement {
  connectedCallback() {
    const dialog = document.createElement('dialog');
    // TODO this dialog is a popover just for light dismiss. if light dismiss
    // for dialogs ever gets implemented, then use that here instead.
    dialog.setAttribute('popover', 'auto');
    dialog.onclose = () => dialog.remove();
    this.appendChild(dialog);

    const closeButton = document.createElement('button');
    closeButton.textContent = 'Close menu';
    dialog.appendChild(closeButton);
    closeButton.onclick = () => {
      dialog.hidePopover();
      this.remove();
    };

    // TODO call syncfs from C when the save is actually written internally
    // so we can get rid of this button completely.
    const saveButton = document.createElement('button');
    saveButton.textContent = 'Save';
    dialog.appendChild(saveButton);
    saveButton.onclick = async () => {
      saveButton.disabled = true;
      await FileLoader.writefs();
      saveButton.disabled = false;
    };

    const saveStateButton = document.createElement('button');
    saveStateButton.textContent = 'Save State';
    dialog.appendChild(saveStateButton);
    saveStateButton.onclick = async () => {
      saveStateButton.disabled = true;
      window.Module._saveState(1);
      await FileLoader.writefs();
      saveStateButton.disabled = false;
    };

    const loadStateButton = document.createElement('button');
    loadStateButton.textContent = 'Load State';
    dialog.appendChild(loadStateButton);
    loadStateButton.onclick = () => {
      window.Module._loadState(1);
    };

    const controlsToggleButton = document.createElement('button');
    controlsToggleButton.textContent = 'Toggle Buttons';
    dialog.appendChild(controlsToggleButton);
    controlsToggleButton.onclick = () => {
      const game = document.querySelector('mgba-game');
      game.toggleTouchControls();
    };

    /*const settingsButton = document.createElement('button');
    settingsButton.textContent = 'Settings';
    settingsButton.onclick = () => {
      this.remove();
      document.body.appendChild(document.createElement('mgba-settings-dialog'));
    };
    dialog.appendChild(settingsButton);*/

    const quitButton = document.createElement('button');
    quitButton.textContent = 'Quit';
    dialog.appendChild(quitButton);
    quitButton.onclick = () => {
      try {
        window.Module._quitGame();
      } catch (e) {}
      dialog.hidePopover();
      this.remove();
      document.querySelector('mgba-game').remove();
      document.body.appendChild(document.createElement('mgba-menu'));
    }

    const volumeContainer = document.createElement('div');
    volumeContainer.style = 'display: inline-block; border: 1px solid gray; padding: 3px;';
    dialog.appendChild(volumeContainer);
    const volumeLabel = document.createElement('label');
    volumeLabel.textContent = 'Volume';
    volumeContainer.appendChild(volumeLabel);
    const volumeInput = document.createElement('input');
    volumeLabel.appendChild(volumeInput);
    volumeInput.type = 'range';
    volumeInput.min = 0;
    volumeInput.max = 0x100;
    volumeInput.defaultValue = window.Module._getVolume();
    volumeInput.oninput = () => {
      window.Module._setVolume(volumeInput.value);
      localStorage.setItem('volume', window.Module._getVolume());
    };

    const muteButton = document.createElement('button');
    muteButton.textContent = 'Mute';
    dialog.appendChild(muteButton);
    muteButton.onclick = () => {
      window.Module._setVolume(0);
      volumeInput.value = 0;
      localStorage.setItem('volume', 0);
    };

    dialog.showPopover();
  }
};

customElements.define('mgba-game-menu', MgbaGameMenu);

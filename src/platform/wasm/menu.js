import MgbaGame from './game.js';
import MgbaSettingsDialog from './settings.js';
import * as FileLoader from './fileloader.js';
import MgbaStorage from './storage.js';

export default class MgbaMenu extends HTMLElement {
  async connectedCallback() {
    const filepicker = document.createElement('button');
    filepicker.textContent = 'pick a GBA ROM file...';
    this.appendChild(filepicker);
    filepicker.onclick = () => {
      const fileInput = document.createElement('input');
      fileInput.type = 'file';
      fileInput.click();
      fileInput.onchange = async () => {
        const file = fileInput.files[0];
        const filepath = `/data/games/${file.name}`;
        await FileLoader.saveFile(filepath, file);

        this.remove();
        const game = document.createElement('mgba-game');
        game.name = file.name;
        document.body.appendChild(game);
      };
    };

    const games = FileLoader.readdirWithoutDotDirs('/data/games');

    if (games.length) {
      const title = document.createElement('h3');
      title.textContent = 'Saved ROMs:';
      this.appendChild(title);
    }
    for (const gameName of games) {
      const button = document.createElement('button');
      button.textContent = gameName;
      this.appendChild(button);
      button.onclick = () => {
        this.remove();
        const mgbaGame = document.createElement('mgba-game');
        mgbaGame.name = gameName;
        document.body.appendChild(mgbaGame);
      };
    }

    this.appendChild(document.createElement('br'));

    const storageButton = document.createElement('button');
    storageButton.textContent = 'Storage Manager';
    this.appendChild(storageButton);
    storageButton.onclick = () => {
      this.remove();
      document.body.appendChild(document.createElement('mgba-storage'));
    };

    this.appendChild(document.createElement('br'));

    const aboutButton = document.createElement('button');
    aboutButton.textContent = 'About';
    this.appendChild(aboutButton);
    aboutButton.onclick = () => {
      const dialog = document.createElement('dialog');
      dialog.onclose = () => dialog.remove();
      this.appendChild(dialog);

      const version = document.createElement('div');
      version.textContent = `${window.Module.version.projectName} ${window.Module.version.projectVersion}`;
      dialog.appendChild(version);

      const commit = document.createElement('div');
      commit.textContent = `commit ${window.Module.version.gitShort}`;
      dialog.appendChild(commit);

      const updateButton = document.createElement('button');
      updateButton.onclick = async () => {
        updateButton.disabled = true;
        try {
          const serviceWorker = await navigator.serviceWorker.ready;
          await serviceWorker.update();
        } catch (error) {
          console.error('serviceWorker error: ', error);
          window.alert('Encountered an error getting the service worker.');
        }
        updateButton.disabled = false;
      };
      updateButton.textContent = 'Attempt update';
      dialog.appendChild(updateButton);

      const closeButton = document.createElement('button');
      closeButton.textContent = 'Close';
      closeButton.onclick = () => {
        dialog.close();
        dialog.remove();
      };
      dialog.appendChild(closeButton);

      dialog.showModal();
    };

    /*const settingsButton = document.createElement('button');
    settingsButton.classList.add('settings');
    settingsButton.textContent = 'Settings';
    this.appendChild(settingsButton);
    settingsButton.onclick = () => {
      document.body.appendChild(document.createElement('mgba-settings-dialog'));
    };*/
  }
};

customElements.define('mgba-menu', MgbaMenu);

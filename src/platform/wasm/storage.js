import MgbaMenu from './menu.js';
import * as FileLoader from './fileloader.js';

export default class MgbaStorage extends HTMLElement {
  connectedCallback() {
    const title = document.createElement('h2');
    title.textContent = 'Storage Manager';
    this.appendChild(title);

    const closeButton = document.createElement('button');
    closeButton.textContent = 'Main menu';
    this.appendChild(closeButton);
    closeButton.onclick = () => {
      this.remove();
      document.body.appendChild(document.createElement('mgba-menu'));
    };

    const uploadButton = document.createElement('button');
    uploadButton.textContent = 'Upload file...';
    this.appendChild(uploadButton);
    uploadButton.onclick = () => {
      const fileInput = document.createElement('input');
      fileInput.type = 'file';
      fileInput.click();
      fileInput.onchange = async () => {
        const file = fileInput.files[0];
        const split = file.name.split('.');
        if (split.length < 2) {
          window.alert('unrecognized file extension: ' + file.name);
          return;
        }
        const extension = split[split.length - 1].toLowerCase();
        let dir = null;
        if (extension === 'gba') {
          dir = '/data/games/';
        } else if (extension == 'sav') {
          dir = '/data/saves/';
        } else if (extension.startsWith('ss')) {
          dir = '/data/states/';
        } else {
          window.alert('unrecognized file extension: ' + extension);
          return;
        }

        const filepath = dir + file.name;
        await FileLoader.saveFile(filepath, file);
        this.refreshFiles();
      };
    };

    const gamesTitle = document.createElement('h3');
    gamesTitle.textContent = 'ROM files:';
    this.appendChild(gamesTitle);
    this.gamesContainer = document.createElement('div');
    this.appendChild(this.gamesContainer);

    const savesTitle = document.createElement('h3');
    savesTitle.textContent = 'Save files:';
    this.appendChild(savesTitle);
    this.savesContainer = document.createElement('div');
    this.appendChild(this.savesContainer);

    const statesTitle = document.createElement('h3');
    statesTitle.textContent = 'Save state files:';
    this.appendChild(statesTitle);
    this.statesContainer = document.createElement('div');
    this.appendChild(this.statesContainer);

    this.refreshFiles();
  }

  refreshFiles() {
    removeAllChildren(this.gamesContainer);
    const games = FileLoader.readdirWithoutDotDirs('/data/games');
    for (const gameName of games) {
      this.appendFileButton(gameName, this.gamesContainer, `/data/games/${gameName}`);
    }

    removeAllChildren(this.savesContainer);
    const saves = FileLoader.readdirWithoutDotDirs('/data/saves');
    for (const saveName of saves) {
      this.appendFileButton(saveName, this.savesContainer, `/data/saves/${saveName}`);
    }

    removeAllChildren(this.statesContainer);
    const states = FileLoader.readdirWithoutDotDirs('/data/states');
    for (const stateName of states) {
      this.appendFileButton(stateName, this.statesContainer, `/data/states/${stateName}`);
    }
  }

  appendFileButton(filename, parent, filepath) {
    const container = document.createElement('div');
    parent.appendChild(container);

    const filesize = document.createElement('span');
    container.appendChild(filesize);
    container.textContent = FileLoader.humanFileSize(
      window.Module.FS.stat(filepath).size) + ' ';

    const button = document.createElement('a');
    container.appendChild(button);
    button.href = '#';
    button.textContent = filename;
    button.onclick = () => {
      const dialog = document.createElement('dialog');
      dialog.onclose = () => dialog.remove();
      this.appendChild(dialog);

      const closeButton = document.createElement('button');
      closeButton.textContent = 'Close';
      dialog.appendChild(closeButton);
      closeButton.onclick = () => {
        dialog.close();
        dialog.remove();
      };

      const fileNameLabel = document.createElement('div');
      fileNameLabel.textContent = 'Filename:';
      dialog.appendChild(fileNameLabel);
      const fileName = document.createElement('div');
      fileName.textContent = filename;
      dialog.appendChild(fileName);

      const downloadButton = document.createElement('button');
      downloadButton.textContent = 'Download';
      dialog.appendChild(downloadButton);
      downloadButton.onclick = () => {
        FileLoader.downloadFile(filepath, filename);
      };

      dialog.appendChild(document.createElement('br'));

      const deleteButton = document.createElement('button');
      dialog.appendChild(deleteButton);
      deleteButton.textContent = 'Delete';
      deleteButton.onclick = async () => {
        if (window.confirm('Delete this file? ' + filename)) {
          window.Module.FS.unlink(filepath);
          await FileLoader.writefs();
          dialog.close();
          dialog.remove();
          this.refreshFiles();
        }
      };

      dialog.appendChild(document.createElement('br'));

      const renameButton = document.createElement('button');
      dialog.appendChild(renameButton);
      renameButton.textContent = 'Rename';
      renameButton.onclick = async () => {
        const newFilename = window.prompt('Enter new filename for ' + filename);
        if (newFilename) {
          window.Module.FS.rename(filepath, filepath.replace(filename, newFilename));
          await FileLoader.writefs();
          dialog.close();
          dialog.remove();
          this.refreshFiles();
        }
      };

      dialog.showModal();
    };
  }
};

function removeAllChildren(node) {
  while (node.firstChild) {
    node.lastChild.remove();
  }
}

customElements.define('mgba-storage', MgbaStorage);

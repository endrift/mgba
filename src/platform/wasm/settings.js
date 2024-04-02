export default class MgbaSettingsDialog extends HTMLElement {
  connectedCallback() {
    const dialog = document.createElement('dialog');
    dialog.onclose = () => dialog.remove();
    this.appendChild(dialog);

    const title = document.createElement('h1');
    title.textContent = 'Settings';
    title.style = `
      text-align: center;
    `;
    dialog.appendChild(title);

    const closeButton = document.createElement('button');
    closeButton.textContent = 'Close settings';
    dialog.appendChild(closeButton);
    closeButton.onclick = () => {
      dialog.close();
      this.remove();
    };

    /*const volumeLabel = document.createElement('label');
    volumeLabel.textContent = 'Volume';
    dialog.appendChild(volumeLabel);
    const volumeInput = document.createElement('input');
    volumeLabel.appendChild(volumeInput);
    volumeInput.type = 'range';
    volumeInput.min = 0;
    volumeInput.max = 0x100;
    volumeInput.oninput = () => {
      console.log('volumeInput.value: ' + volumeInput.value);
      window.Module._setVolume(volumeInput.value);
      localStorage.setItem('volume', window.Module._getVolume());
    };*/

    dialog.showModal();
  }
};

customElements.define('mgba-settings-dialog', MgbaSettingsDialog);

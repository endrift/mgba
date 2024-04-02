export function loadGame(filepath) {
	const cLoadGame = window.Module.cwrap('loadGame', 'number', ['string']);
  if (!cLoadGame(filepath)) {
    const message = 'Module._loadGame returned false! filepath: ' + filepath;
    console.log(message);
    throw new Error(message);
  }
}

export async function saveFile(filepath, file) {
  return new Promise(resolve => {
    const reader = new FileReader();
    reader.onload = async e => {
      window.Module.FS.writeFile(
        filepath, new Uint8Array(e.target.result));
      await writefs();
      resolve();
    }
    reader.readAsArrayBuffer(file);
  });
}

export function downloadFile(filepath, filename) {
  const save = window.Module.FS.readFile(filepath);
  const a = document.createElement('a');
  document.body.appendChild(a);
  a.download = filename;
  const blob = new Blob([save], { type: 'application/octet-stream' });
  a.href = URL.createObjectURL(blob);

  a.click();

  URL.revokeObjectURL(blob);
  a.remove();
}

export async function readfs() {
  const err = await new Promise(resolve => {
    window.Module.FS.syncfs(/*populate=*/true, resolve);
  });
  if (err)
    console.log('syncfs error: ', err);
}

export async function writefs() {
  const err = await new Promise(resolve => {
    window.Module.FS.syncfs(/*populate=*/false, resolve);
  });
  if (err)
    console.log('syncfs error: ', err);
}

export function readdirWithoutDotDirs(path) {
  const files = window.Module.FS.readdir(path);
  files.splice(files.indexOf('.'), 1);
  files.splice(files.indexOf('..'), 1);
  return files;
}

// https://stackoverflow.com/a/14919494
export function humanFileSize(bytes, si=false, dp=1) {
  const thresh = si ? 1000 : 1024;

  if (Math.abs(bytes) < thresh) {
    return bytes + ' B';
  }

  const units = si 
    ? ['kB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'] 
    : ['KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB'];
  let u = -1;
  const r = 10**dp;

  do {
    bytes /= thresh;
    ++u;
  } while (Math.round(Math.abs(bytes) * r) / r >= thresh && u < units.length - 1);


  return bytes.toFixed(dp) + ' ' + units[u];
}

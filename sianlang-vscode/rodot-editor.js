'use strict';

const crypto = require('node:crypto');

const MAGIC = Buffer.from('RODOT001', 'ascii');
const PNG_MAGIC = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

function parseRodot(input) {
  const bytes = Buffer.from(input);
  if (bytes.length < 36 || !bytes.subarray(0, 8).equals(PNG_MAGIC) ||
      bytes.toString('ascii', 12, 16) !== 'IHDR') {
    throw new Error('This is not a valid PNG-based .rodot file.');
  }
  const width = bytes.readUInt32BE(16);
  const height = bytes.readUInt32BE(20);
  if (!width || !height || width > 16384 || height > 16384) {
    throw new Error('Invalid PNG dimensions in .rodot file.');
  }
  if (!bytes.subarray(-8).equals(MAGIC)) {
    throw new Error('Missing .rodot JSON trailer.');
  }
  const length = bytes.readUInt32LE(bytes.length - 12);
  const start = bytes.length - 12 - length;
  if (!length || length > 16 * 1024 * 1024 || start < 24) {
    throw new Error('Invalid .rodot JSON length.');
  }
  let metadata;
  try {
    metadata = JSON.parse(bytes.subarray(start, start + length).toString('utf8'));
  } catch {
    throw new Error('Invalid JSON in .rodot file.');
  }
  if (!metadata || Array.isArray(metadata) || typeof metadata !== 'object' ||
      typeof metadata.name !== 'string' || !metadata.meta ||
      metadata.meta.width !== width || metadata.meta.height !== height ||
      !metadata.data || Array.isArray(metadata.data) || typeof metadata.data !== 'object') {
    throw new Error('.rodot name, dimensions, or data are invalid.');
  }
  return { image: bytes.subarray(0, start), metadata, width, height };
}

function parseData(text) {
  let value;
  try { value = JSON.parse(text); } catch {
    throw new Error('data must contain valid JSON.');
  }
  if (!value || Array.isArray(value) || typeof value !== 'object') {
    throw new Error('data must be a JSON object.');
  }
  return value;
}

function serializeRodot(parsed, dataText) {
  const metadata = { ...parsed.metadata, data: parseData(dataText) };
  const json = Buffer.from(JSON.stringify(metadata), 'utf8');
  if (!json.length || json.length > 16 * 1024 * 1024) {
    throw new Error('.rodot JSON is too large.');
  }
  const length = Buffer.alloc(4);
  length.writeUInt32LE(json.length);
  return Buffer.concat([parsed.image, json, length, MAGIC]);
}

function editorHtml(nonce) {
  return `<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src data:; style-src 'unsafe-inline'; script-src 'nonce-${nonce}'">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: var(--vscode-font-family); color: var(--vscode-foreground); padding: 18px; }
header { display: flex; align-items: center; gap: 20px; margin-bottom: 16px; }
img { width: 128px; height: 128px; object-fit: contain; image-rendering: pixelated; background: #555; }
textarea { width: 100%; box-sizing: border-box; min-height: 340px; resize: vertical;
  background: var(--vscode-editor-background); color: var(--vscode-editor-foreground);
  border: 1px solid var(--vscode-input-border); padding: 12px;
  font-family: var(--vscode-editor-font-family); font-size: var(--vscode-editor-font-size); }
small { color: var(--vscode-descriptionForeground); }
#status.error { color: var(--vscode-errorForeground); }
</style></head><body>
<header><img id="preview" alt="Sprite preview"><div><h2 id="name"></h2><div id="size"></div>
<small>Image, name and original dimensions are read-only.</small></div></header>
<label for="data">Sprite data (JSON)</label><textarea id="data" spellcheck="false"></textarea>
<p id="status">Edit data and press Ctrl+S to save the .rodot file.</p>
<script nonce="${nonce}">
const vscode = acquireVsCodeApi();
const input = document.getElementById('data');
const status = document.getElementById('status');
window.addEventListener('message', event => {
  const message = event.data;
  if (message.type !== 'state') return;
  document.getElementById('preview').src = 'data:image/png;base64,' + message.image;
  document.getElementById('name').textContent = message.name;
  document.getElementById('size').textContent = message.width + ' × ' + message.height;
  input.value = message.data;
  status.textContent = 'Edit data and press Ctrl+S to save the .rodot file.';
  status.className = '';
});
input.addEventListener('input', () => {
  vscode.postMessage({ type: 'edit', text: input.value });
  try {
    const parsed = JSON.parse(input.value);
    if (!parsed || Array.isArray(parsed) || typeof parsed !== 'object') throw new Error();
    status.textContent = 'Unsaved changes'; status.className = '';
  } catch {
    status.textContent = 'data must be a valid JSON object before saving.';
    status.className = 'error';
  }
});
vscode.postMessage({ type: 'ready' });
</script></body></html>`;
}

function registerRodotEditor(vscode, context) {
  const changes = new vscode.EventEmitter();
  const provider = {
    onDidChangeCustomDocument: changes.event,
    async openCustomDocument(uri, openContext) {
      let parsed, dataText;
      if (openContext.backupId) {
        const backup = JSON.parse(Buffer.from(await vscode.workspace.fs.readFile(
          vscode.Uri.parse(openContext.backupId))).toString('utf8'));
        parsed = parseRodot(Buffer.from(backup.original, 'base64'));
        dataText = backup.dataText;
      } else {
        parsed = parseRodot(await vscode.workspace.fs.readFile(uri));
        dataText = JSON.stringify(parsed.metadata.data, null, 2);
      }
      return { uri, parsed, dataText,
        panels: new Set(),
        dispose() {} };
    },
    async resolveCustomEditor(document, panel) {
      panel.webview.options = { enableScripts: true };
      panel.webview.html = editorHtml(crypto.randomBytes(16).toString('hex'));
      const sendState = () => panel.webview.postMessage({ type: 'state',
        image: document.parsed.image.toString('base64'),
        name: document.parsed.metadata.name, width: document.parsed.width,
        height: document.parsed.height, data: document.dataText });
      document.panels.add(sendState);
      panel.onDidDispose(() => document.panels.delete(sendState));
      panel.webview.onDidReceiveMessage(message => {
        if (message.type === 'ready') { sendState(); return; }
        if (message.type === 'edit' && typeof message.text === 'string' &&
            message.text !== document.dataText) {
          document.dataText = message.text;
          changes.fire({ document });
        }
      });
    },
    async saveCustomDocument(document) {
      const current = parseRodot(await vscode.workspace.fs.readFile(document.uri));
      if (!current.image.equals(document.parsed.image) ||
          JSON.stringify(current.metadata) !== JSON.stringify(document.parsed.metadata)) {
        throw new Error('.rodot changed outside the editor. Reopen it before saving.');
      }
      await writeRodot(vscode, document.uri, serializeRodot(document.parsed, document.dataText));
      document.parsed = parseRodot(await vscode.workspace.fs.readFile(document.uri));
    },
    async saveCustomDocumentAs(document, destination) {
      await writeRodot(vscode, destination, serializeRodot(document.parsed, document.dataText));
    },
    async revertCustomDocument(document) {
      document.parsed = parseRodot(await vscode.workspace.fs.readFile(document.uri));
      document.dataText = JSON.stringify(document.parsed.metadata.data, null, 2);
      for (const sendState of document.panels) sendState();
    },
    async backupCustomDocument(document, context) {
      await vscode.workspace.fs.writeFile(context.destination, Buffer.from(JSON.stringify({
        original: serializeRodot(document.parsed,
          JSON.stringify(document.parsed.metadata.data)).toString('base64'),
        dataText: document.dataText
      }), 'utf8'));
      return { id: context.destination.toString(), delete: () => vscode.workspace.fs.delete(context.destination) };
    }
  };
  context.subscriptions.push(changes,
    vscode.window.registerCustomEditorProvider('sianlang.rodotEditor', provider));
}

async function writeRodot(vscode, destination, bytes) {
  const temporary = destination.with({ path: destination.path + '.sian-tmp-' + crypto.randomBytes(8).toString('hex') });
  try {
    await vscode.workspace.fs.writeFile(temporary, bytes);
    await vscode.workspace.fs.rename(temporary, destination, { overwrite: true });
  } catch (error) {
    try { await vscode.workspace.fs.delete(temporary); } catch { /* No temp file to remove. */ }
    throw error;
  }
}

module.exports = { parseRodot, serializeRodot, registerRodotEditor };

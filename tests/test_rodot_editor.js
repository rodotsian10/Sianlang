'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { parseRodot, serializeRodot, registerRodotEditor } = require('../sianlang-vscode/rodot-editor');

const original = fs.readFileSync(path.join(__dirname, '..', 'sianlang1stgame', 'player.rodot'));
const parsed = parseRodot(original);
assert.equal(parsed.metadata.name, 'player');
assert.equal(parsed.width, 128);
assert.equal(parsed.height, 128);
const changed = serializeRodot(parsed, '{"x": 77, "visible": false}');
const reloaded = parseRodot(changed);
assert.deepEqual(reloaded.image, parsed.image);
assert.equal(reloaded.metadata.meta.width, parsed.metadata.meta.width);
assert.equal(reloaded.metadata.data.x, 77);
assert.throws(() => serializeRodot(parsed, '{broken'), /valid JSON/);
assert.throws(() => serializeRodot(parsed, '[]'), /JSON object/);
assert.throws(() => parseRodot(original.subarray(0, -1)), /trailer/);

class Uri {
  constructor(value) { this.path = value; }
  with({ path: next }) { return new Uri(next); }
  toString() { return this.path; }
  static parse(value) { return new Uri(value); }
}
const resource = new Uri('/sprite.rodot');
const files = new Map([[resource.path, original]]);
const posts = [];
let receiver, provider;
let changedEvents = 0;
const vscode = {
  Uri,
  EventEmitter: class {
    constructor() { this.event = () => {}; }
    fire() { changedEvents++; }
  },
  window: { registerCustomEditorProvider: (_, instance) => { provider = instance; return {}; } },
  workspace: { fs: {
    async readFile(uri) { if (!files.has(uri.path)) throw new Error('missing'); return files.get(uri.path); },
    async writeFile(uri, bytes) { files.set(uri.path, Buffer.from(bytes)); },
    async rename(from, to) { files.set(to.path, files.get(from.path)); files.delete(from.path); },
    async delete(uri) { files.delete(uri.path); }
  } }
};
registerRodotEditor(vscode, { subscriptions: [] });
const panel = { webview: {
  postMessage: message => { posts.push(message); },
  onDidReceiveMessage: callback => { receiver = callback; },
  options: {}, html: ''
}, onDidDispose: () => {} };

(async () => {
  const document = await provider.openCustomDocument(resource, {});
  await provider.resolveCustomEditor(document, panel);
  receiver({ type: 'ready' });
  assert.equal(posts.at(-1).name, 'player');
  assert.ok(posts.at(-1).image.length > 10);
  receiver({ type: 'edit', text: '{"x": 77}' });
  assert.equal(changedEvents, 1);
  await provider.saveCustomDocument(document);
  const saved = parseRodot(files.get(resource.path));
  assert.deepEqual(saved.image, parsed.image);
  assert.equal(saved.metadata.data.x, 77);
  receiver({ type: 'edit', text: '{"x": 88}' });
  receiver({ type: 'edit', text: '{"x":' });
  const backup = await provider.backupCustomDocument(document, { destination: new Uri('/draft.backup') });
  const restored = await provider.openCustomDocument(resource, { backupId: backup.id });
  assert.equal(restored.dataText, '{"x":');
  await backup.delete();
  await provider.revertCustomDocument(document);
  assert.equal(posts.at(-1).data.includes('77'), true);
  files.set(resource.path, original);
  receiver({ type: 'edit', text: '{"x": 99}' });
  await assert.rejects(provider.saveCustomDocument(document), /changed outside/);
  console.log('Rodot editor preview, JSON edits, save, revert, and conflict checks passed.');
})().catch(error => { console.error(error); process.exitCode = 1; });

'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

async function scenario(options = {}) {
  let callback;
  const errors = [], tasks = [];
  const root = path.resolve(__dirname, '..');
  const file = options.file || path.join(root, "space & dollar$ back` quote' file.sian");
  const folder = { uri: { fsPath: root } };
  const api = {
    commands: { registerCommand: (_, cb) => { callback = cb; return {}; } },
    window: {
      registerCustomEditorProvider: () => ({}),
      activeTextEditor: options.noEditor ? undefined : { document: {
        languageId: 'sianlang', uri: { scheme: options.remote ? 'vscode-vfs' : 'file' }, fileName: file,
        isUntitled: !!options.untitled,
        save: async () => {
          if (options.saveError) throw new Error('save failed');
          return !options.saveFalse;
        }
      } },
      showErrorMessage: text => errors.push(text)
    },
    workspace: {
      isTrusted: !options.untrusted,
      getWorkspaceFolder: () => options.noFolder ? undefined : folder,
      getConfiguration: () => ({ get: () => options.configured || '' })
    },
    ProcessExecution: class { constructor(command, args, options) { Object.assign(this, { command, args, options }); } },
    EventEmitter: class { constructor() { this.event = () => {}; } fire() {} },
    Task: class { constructor(definition, scope, name, source, execution) { Object.assign(this, { definition, scope, name, source, execution }); } },
    TaskRevealKind: { Always: 1 }, TaskPanelKind: { Dedicated: 2 }, TaskScope: { Global: 1 },
    tasks: { executeTask: async task => {
      if (options.runError) throw new Error('process failed');
      tasks.push(task);
    } }
  };
  const module = { exports: {} };
  const source = fs.readFileSync(path.join(root, 'sianlang-vscode', 'extension.js'), 'utf8');
  vm.runInNewContext(source, {
    module,
    process: { platform: options.linux ? 'linux' : 'win32', arch: 'x64' },
    require: name => name === './rodot-editor' ? require(path.join(root, 'sianlang-vscode', 'rodot-editor.js')) : name === 'vscode' ? api : name === 'fs' ? {
      statSync: candidate => {
        if (options.missing) throw new Error('missing');
        return { isFile: () => !options.directory };
      }
    } : require(name)
  });
  module.exports.activate({ subscriptions: [], extensionPath: path.join(root, 'sianlang-vscode') });
  await callback();
  return { errors, tasks, file, root };
}

(async () => {
  let count = 0;
  const good = await scenario();
  assert.equal(good.errors.length, 0);
  assert.equal(good.tasks.length, 1);
  assert.equal(good.tasks[0].execution.command, path.join(good.root, 'sianlang-vscode', 'bin', 'win32-x64', 'Sianlang.exe'));
  assert.equal(good.tasks[0].execution.args.length, 1);
  assert.equal(good.tasks[0].execution.args[0], good.file);
  assert.equal(good.tasks[0].execution.options.cwd, path.dirname(good.file));
  assert.equal(good.tasks[0].presentationOptions.focus, true);
  count++;
  for (const option of ['noEditor', 'saveFalse', 'saveError', 'missing', 'directory', 'runError', 'untrusted', 'remote', 'untitled', 'linux']) {
    const result = await scenario({ [option]: true });
    assert.equal(result.tasks.length, 0, option);
    assert.equal(result.errors.length, 1, option);
    count++;
  }
  for (const options of [{ noFolder: true }, { configured: 'custom/Sianlang.exe' }, { configured: path.join(good.root, 'custom.exe'), linux: true }]) {
    const result = await scenario(options);
    assert.equal(result.errors.length, 0);
    assert.equal(result.tasks.length, 1);
    if (options.configured) assert.equal(result.tasks[0].execution.command, path.resolve(good.root, options.configured));
    if (options.noFolder) assert.equal(result.tasks[0].scope, 1);
    count++;
  }
  const nested = await scenario({ file: path.join(good.root, 'sianlanggameex1', 'game.sian') });
  assert.equal(nested.tasks[0].execution.options.cwd, path.join(good.root, 'sianlanggameex1'));
  count++;
  console.log(`${count} extension scenarios passed`);
})().catch(error => { console.error(error); process.exitCode = 1; });

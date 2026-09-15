// Runs inside the real VS Code Extension Host, with an isolated test profile.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vscode = require('vscode');

exports.run = async () => {
  const root = path.resolve(__dirname, '..');
  const report = path.join(root, 'build', 'extension-host-result.json');
  try {
    const extension = vscode.extensions.getExtension('sianlang.sianlang-vscode');
    assert.ok(extension, 'Packaged extension discovered');
    await extension.activate();
    assert.ok(extension.isActive);
    assert.equal(extension.packageJSON.version, '0.3.1');
    const uri = vscode.Uri.file(path.join(root, '배포', 'hello.sian'));
    const document = await vscode.workspace.openTextDocument(uri);
    assert.equal(document.languageId, 'sianlang');
    await vscode.window.showTextDocument(document);
    assert.ok(vscode.workspace.isTrusted);
    const result = await new Promise((resolve, reject) => {
      const timer = setTimeout(() => { listener.dispose(); reject(new Error('SianLang task did not finish within 20s')); }, 20000);
      const listener = vscode.tasks.onDidEndTaskProcess(event => {
        if (event.execution.task.source !== 'SianLang') return;
        clearTimeout(timer); listener.dispose();
        resolve({ exitCode: event.exitCode, command: event.execution.task.execution.process });
      });
      vscode.commands.executeCommand('sianlang.runFile').then(undefined, error => {
        clearTimeout(timer); listener.dispose(); reject(error);
      });
    });
    assert.equal(result.exitCode, 0, JSON.stringify(result));
    assert.ok(result.command.includes(path.join('bin', 'win32-x64')));
    const theme = extension.packageJSON.contributes.iconThemes[0];
    const icons = JSON.parse(fs.readFileSync(path.join(extension.extensionPath, theme.path), 'utf8'));
    assert.equal(icons.fileExtensions.sian, '_sianlang');
    assert.deepEqual(fs.readFileSync(path.join(extension.extensionPath, 'icons', 'sianlang-file.svg')),
                     fs.readFileSync(path.join(root, 'Sianlangicon.svg')));
    fs.writeFileSync(report, JSON.stringify({ passed: true, version: extension.packageJSON.version, ...result }, null, 2));
  } catch (error) {
    fs.writeFileSync(report, JSON.stringify({ passed: false, error: error.stack }, null, 2));
    throw error;
  }
};

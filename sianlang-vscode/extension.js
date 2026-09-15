const vscode = require('vscode');
const fs = require('fs');
const path = require('path');

function activate(context) {
  const runFile = vscode.commands.registerCommand('sianlang.runFile', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'sianlang') {
      vscode.window.showErrorMessage('Open a .sian file first.');
      return;
    }

    const document = editor.document;
    if (!vscode.workspace.isTrusted) {
      vscode.window.showErrorMessage('Trust this workspace before running SianLang.');
      return;
    }
    if (document.isUntitled || document.uri.scheme !== 'file') {
      vscode.window.showErrorMessage('Save this program as a local .sian file first.');
      return;
    }
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(document.uri);
    const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(document.fileName);

    try {
      if (!(await document.save())) {
        vscode.window.showErrorMessage('The file could not be saved. Run cancelled.');
        return;
      }
      const configured = vscode.workspace.getConfiguration('sianlang', document.uri).get('executable', '').trim();
      if (!configured && (process.platform !== 'win32' || process.arch !== 'x64')) {
        vscode.window.showErrorMessage('The bundled interpreter requires Windows x64. Set sianlang.executable to a native interpreter on this system.');
        return;
      }
      const executableCandidates = [configured
        ? (path.isAbsolute(configured) ? configured : path.resolve(cwd, configured))
        : path.join(context.extensionPath, 'bin', 'win32-x64', 'Sianlang.exe')];
      const executable = executableCandidates.find((candidate) => {
        try { return fs.statSync(candidate).isFile(); } catch { return false; }
      });

      if (!executable) {
        vscode.window.showErrorMessage(`Interpreter not found: ${executableCandidates[0]}`);
        return;
      }

      // A process task passes paths as arguments without shell interpolation.
      // Its terminal remains interactive, including input() and Ctrl+C.
      const execution = new vscode.ProcessExecution(executable, [document.fileName], {
        cwd
      });
      const task = new vscode.Task({ type: 'sianlang' }, workspaceFolder || vscode.TaskScope.Global,
        'Run File', 'SianLang', execution, []);
      task.presentationOptions = {
        reveal: vscode.TaskRevealKind.Always,
        panel: vscode.TaskPanelKind.Dedicated,
        focus: true,
        clear: true
      };
      await vscode.tasks.executeTask(task);
    } catch (error) {
      vscode.window.showErrorMessage(`Unable to run SianLang: ${error.message || error}`);
    }
  });

  context.subscriptions.push(runFile);
}

function deactivate() {}

module.exports = { activate, deactivate };

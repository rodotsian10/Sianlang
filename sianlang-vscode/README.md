# SianLang Language Support 0.3.1

Syntax highlighting, indentation, file icons, and interactive execution for `.sian` files.

## Run a file

1. Download `sianlang-vscode-0.3.1.vsix`. It includes the Windows x64 interpreter; no compiler, Python, or Node.js installation is required.
2. Install it using VS Code **Extensions: Install from VSIX...**.
3. Open a saved local `.sian` file in a trusted workspace. Standalone files also work.
4. Press F6 or use **SianLang: Run File**. The file is saved before execution.

Leave `sianlang.executable` empty to use the bundled interpreter. A custom path is absolute or relative to the workspace (the file's folder for standalone files). macOS/Linux/remote environments need their own native interpreter. This release is tested on Windows x64.

Execution uses a process task with separate arguments. Paths with spaces and shell characters are not interpreted as commands. The terminal accepts input() and Ctrl+C. Failed saves cancel execution and show an error.

## Icons

Select **Preferences: File Icon Theme → SianLang File Icons** to enable the .sian file icon. The extension also has an icon in the Extensions list; it does not add an Activity Bar view.

The file icon uses the creator's `Sianlangicon.svg`. VS Code enables one file icon theme at a time: selecting this theme replaces Material Icon Theme rather than adding to it.

A default language icon is also registered. Existing themes can display it when they allow language icons and do not override the `.sian` association.

## Snippets and limits

Type `log`, `logf`, `def`, `if`, `while`, or `try` to select a snippet. Language-server diagnostics, semantic completion, and debugger integration are not included yet.

## Development and packaging

From the project root:

    node tests/test_extension.js
    python tools/package_extension.py

The local packager verifies the archive contents and version. To debug interactively, open this extension folder and press F5; `.vscode/launch.json` provides an Extension Development Host configuration.

Interpreter rules: [SianLang reference](docs/grammar.md).

param([string]$CodePath = "$env:LOCALAPPDATA\Programs\Microsoft VS Code\Code.exe")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -LiteralPath "$root\sianlang-vscode\package.json" -Raw | ConvertFrom-Json
$version = $manifest.version
$extensionPath = "$root\build\vscode-release-extensions\sianlang.sianlang-vscode-$version"
if (-not (Test-Path -LiteralPath "$extensionPath\package.json")) {
    throw 'Install the release VSIX into build/vscode-release-extensions first. See tests/README.md.'
}
$report = "$root\build\extension-host-result.json"
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report }
$oldNodeMode = $env:ELECTRON_RUN_AS_NODE
try {
    $env:ELECTRON_RUN_AS_NODE = $null
    $arguments = @(
        "--user-data-dir=`"$root\build\vscode-host-profile`"",
        "--extensions-dir=`"$root\build\vscode-release-extensions`"",
        "--extensionDevelopmentPath=`"$extensionPath`"",
        "--extensionTestsPath=`"$root\tests\extension_host.js`"",
        '--disable-workspace-trust', '--skip-welcome', '--skip-release-notes',
        '--disable-updates', '--disable-telemetry', "`"$root`""
    )
    $testProcess = Start-Process -FilePath $CodePath -WindowStyle Hidden -ArgumentList $arguments -PassThru
    if (-not $testProcess.WaitForExit(55000)) {
        Stop-Process -Id $testProcess.Id
        throw 'Extension Host test timed out.'
    }
    if (-not (Test-Path -LiteralPath $report)) { throw 'Extension Host exited without a result.' }
    $result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
    if (-not $result.passed) { throw $result.error }
    Get-Content -LiteralPath $report
} finally {
    $env:ELECTRON_RUN_AS_NODE = $oldNodeMode
}

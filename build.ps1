param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    & gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror -O2 main.c -o build/Sianlang.exe
    if ($LASTEXITCODE -ne 0) { throw 'C build failed' }
    if (-not $SkipTests) {
        & python tests/test_runtime.py --exe build/Sianlang.exe
        if ($LASTEXITCODE -ne 0) { throw 'Interpreter tests failed' }
        & python tests/test_v03.py --exe build/Sianlang.exe
        if ($LASTEXITCODE -ne 0) { throw 'Language feature tests failed' }
        & node tests/test_extension.js
        if ($LASTEXITCODE -ne 0) { throw 'Extension tests failed' }
    }
    Copy-Item -LiteralPath build/Sianlang.exe -Destination Sianlang.exe -Force
    Copy-Item -LiteralPath build/Sianlang.exe -Destination SianlangA.exe -Force
    & python tools/package_extension.py
    if ($LASTEXITCODE -ne 0) { throw 'Extension packaging failed' }
    & python tools/package_release.py
    if ($LASTEXITCODE -ne 0) { throw 'Portable release packaging failed' }
    if (-not $SkipTests) {
        & python tests/test_release.py
        if ($LASTEXITCODE -ne 0) { throw 'Release verification failed' }
    }
    Write-Output 'Built SianLang 0.3.1 (Sianlang.exe and compatibility copy SianlangA.exe)'
} finally {
    Pop-Location
}

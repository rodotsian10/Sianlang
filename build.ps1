param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    & gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror -O2 main.c -o build/Sianlang.exe -lgdi32 -lgdiplus -lshlwapi
    if ($LASTEXITCODE -ne 0) { throw 'C build failed' }
    if (-not $SkipTests) {
        & py tests/test_runtime.py --exe build/Sianlang.exe
        if ($LASTEXITCODE -ne 0) { throw 'Interpreter tests failed' }
        & py tests/test_v03.py --exe build/Sianlang.exe
        if ($LASTEXITCODE -ne 0) { throw 'Language feature tests failed' }
        & py tests/test_v04.py --exe build/Sianlang.exe
        if ($LASTEXITCODE -ne 0) { throw 'Collection feature tests failed' }
        & py tests/test_game.py --exe build/Sianlang.exe
        if ($LASTEXITCODE -ne 0) { throw 'Game feature tests failed' }
        & gcc -std=c11 -Wall -Wextra -Wpedantic -Werror tests/rodot-decode.c -o build/rodot-decode.exe -lgdiplus -lshlwapi
        if ($LASTEXITCODE -ne 0) { throw 'Rodot decoder test build failed' }
        & build/rodot-decode.exe
        if ($LASTEXITCODE -ne 0) { throw 'Rodot decoder test failed' }
        & node tests/test_extension.js
        if ($LASTEXITCODE -ne 0) { throw 'Extension tests failed' }
        & node tests/test_rodot_editor.js
        if ($LASTEXITCODE -ne 0) { throw 'Rodot editor tests failed' }
    }
    Copy-Item -LiteralPath build/Sianlang.exe -Destination Sianlang.exe -Force
    Copy-Item -LiteralPath build/Sianlang.exe -Destination SianlangA.exe -Force
    & py tools/package_extension.py
    if ($LASTEXITCODE -ne 0) { throw 'Extension packaging failed' }
    & py tools/package_release.py
    if ($LASTEXITCODE -ne 0) { throw 'Portable release packaging failed' }
    if (-not $SkipTests) {
        & py tests/test_release.py
        if ($LASTEXITCODE -ne 0) { throw 'Release verification failed' }
    }
    Write-Output 'Built SianLang 0.5.0 (Sianlang.exe and compatibility copy SianlangA.exe)'
} finally {
    Pop-Location
}

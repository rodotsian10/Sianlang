"""Create portable Windows artifacts and a static download site from an explicit whitelist."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parent.parent
meta = json.loads((ROOT / 'sianlang-vscode/package.json').read_text(encoding='utf-8'))
version = meta['version']
runtime = ROOT / 'build/Sianlang.exe'
assert subprocess.check_output([str(runtime), '--version'], text=True).strip() == f'SianLang {version}'
out = ROOT / 'dist' / version
out.mkdir(parents=True, exist_ok=True)
vsix_name = f"{meta['name']}-{version}.vsix"
shutil.copyfile(ROOT / 'sianlang-vscode' / vsix_name, out / vsix_name)
files = {
    'Sianlang.exe': runtime,
    'LICENSE': ROOT / 'LICENSE',
    'START-HERE.md': ROOT / '배포/시작하기.md',
    'CHANGELOG.md': ROOT / 'CHANGELOG.md',
    'game-api.md': ROOT / 'docs/game-api.md',
    'grammar.md': ROOT / '설명서/시안랭-문법규칙서.md',
    'examples/hello.sian': ROOT / 'examples/hello.sian',
    'examples/guessing-game.sian': ROOT / 'examples/guessing-game.sian',
    'examples/dungeon-rpg.sian': ROOT / 'examples/dungeon-rpg.sian',
    'examples/collections-demo.sian': ROOT / 'examples/collections-demo.sian',
    'examples/file-io-demo.sian': ROOT / 'examples/file-io-demo.sian',
    'examples/fjson-demo.sian': ROOT / 'examples/fjson-demo.sian',
    'sianlanggameex1/README.md': ROOT / 'sianlanggameex1/README.md',
    'sianlanggameex1/Conversion.sian': ROOT / 'sianlanggameex1/Conversion.sian',
    'sianlanggameex1/game.sian': ROOT / 'sianlanggameex1/game.sian',
    'sianlanggameex1/sprite.png': ROOT / 'sianlanggameex1/sprite.png',
    vsix_name: out / vsix_name,
}
zip_name = f'SianLang-{version}-windows-x64.zip'
prefix = f'SianLang-{version}'
with ZipFile(out / zip_name, 'w', ZIP_DEFLATED) as archive:
    for target, source in files.items():
        archive.write(source, f'{prefix}/{target}')
with ZipFile(out / zip_name) as archive:
    assert archive.testzip() is None
    for target, source in files.items():
        assert archive.read(f'{prefix}/{target}') == source.read_bytes()
checksums = ''.join(f'{hashlib.sha256((out / name).read_bytes()).hexdigest()}  {name}\n'
                    for name in [zip_name, vsix_name])
(out / 'SHA256SUMS.txt').write_text(checksums, encoding='ascii')
page = (ROOT / '배포/index.html').read_text(encoding='utf-8').replace('{{version}}', version)
(out / 'index.html').write_text(page, encoding='utf-8')
shutil.copyfile(ROOT / 'Sianlangicon.svg', out / 'Sianlangicon.svg')
shutil.copyfile(ROOT / 'docs/commands.html', out / 'commands.html')
site = ROOT / 'docs'
site.mkdir(exist_ok=True)
release_url = f'https://github.com/rodotsian10/Sianlang/releases/download/v{version}/'
for name in [zip_name, vsix_name, 'SHA256SUMS.txt']:
    page = page.replace(f'href="{name}"', f'href="{release_url}{name}"')
(site / 'index.html').write_text(page, encoding='utf-8')
(site / '.nojekyll').touch()
shutil.copyfile(ROOT / 'Sianlangicon.svg', site / 'Sianlangicon.svg')
print(f'Built and verified portable ZIP, VSIX, checksums and download page: {out}', flush=True)

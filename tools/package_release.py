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
    'examples/hello.sian': ROOT / '배포/hello.sian',
    'examples/features-demo.sian': ROOT / 'features-demo.sian',
    'examples/guessing-game.sian': ROOT / 'code.sian',
    'examples/try-catch.sian': ROOT / 'try-catch-test.sian',
    'docs/grammar.md': ROOT / '설명서/시안랭-문법규칙서.md',
    'docs/game-roadmap.md': ROOT / '게임기능-계획.md',
    'CHANGELOG.md': ROOT / 'CHANGELOG.md',
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
site = ROOT / 'docs'
site.mkdir(exist_ok=True)
release_url = f'https://github.com/rodotsian10/Sianlang/releases/download/v{version}/'
for name in [zip_name, vsix_name, 'SHA256SUMS.txt']:
    page = page.replace(f'href="{name}"', f'href="{release_url}{name}"')
(site / 'index.html').write_text(page, encoding='utf-8')
(site / '.nojekyll').touch()
shutil.copyfile(ROOT / 'Sianlangicon.svg', site / 'Sianlangicon.svg')
print(f'Built and verified portable ZIP, VSIX, checksums and download page: {out}')

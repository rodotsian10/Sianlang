"""Validate the delivered artifacts in an isolated Unicode directory, without developer PATH."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from zipfile import ZipFile
from html.parser import HTMLParser

ROOT = Path(__file__).resolve().parent.parent
meta = json.loads((ROOT / 'sianlang-vscode/package.json').read_text(encoding='utf-8'))
version = meta['version']
dist = ROOT / 'dist' / version
for line in (dist / 'SHA256SUMS.txt').read_text().splitlines():
    digest, name = line.split('  ', 1)
    assert hashlib.sha256((dist / name).read_bytes()).hexdigest() == digest

class Links(HTMLParser):
    def handle_starttag(self, tag, attrs):
        for key, value in attrs:
            if key in ('href', 'src'):
                assert (dist / value).is_file(), value
Links().feed((dist / 'index.html').read_text(encoding='utf-8'))

with tempfile.TemporaryDirectory(prefix='배포 확인 & ', dir=ROOT / 'build') as temp:
    with ZipFile(dist / f'SianLang-{version}-windows-x64.zip') as archive:
        archive.extractall(temp)
    portable = Path(temp) / f'SianLang-{version}'
    env = os.environ.copy()
    env['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')
    exe = portable / 'Sianlang.exe'
    def run(args):
        result = subprocess.run([str(exe), *map(str, args)], cwd=portable, env=env,
                                capture_output=True, encoding='utf-8', timeout=20)
        assert result.returncode == 0, result.stderr
        return result.stdout
    assert run(['--version']).strip() == f'SianLang {version}'
    assert 'Hello, SianLang!' in run(['examples/hello.sian'])
    assert '리스트 항목:' in run(['examples/collections-demo.sian'])
    assert 'Fjson 데이터 저장 완료' in run(['examples/fjson-demo.sian'])
    with ZipFile(portable / f"{meta['name']}-{version}.vsix") as extension:
        assert extension.read('extension/bin/win32-x64/Sianlang.exe') == exe.read_bytes()
        assert extension.read('extension/icons/sianlang-file.svg') == (ROOT / 'Sianlangicon.svg').read_bytes()
        packed = json.loads(extension.read('extension/package.json'))
        assert packed['contributes']['languages'][0]['icon']['dark'] == './icons/sianlang-file.svg'
        snippets = json.loads(extension.read('extension/snippets/sianlang.json'))
        assert len(snippets) == 15
        assert extension.read('extension/LICENSE') == (ROOT / 'LICENSE').read_bytes()
    assert (portable / 'LICENSE').read_bytes() == (ROOT / 'LICENSE').read_bytes()
print('Release checks passed: SHA256, download links, clean-PATH portable execution, bundled runtime and icon.')

import hashlib
from pathlib import Path
import urllib.request

root = Path(__file__).resolve().parent.parent
site = 'https://rodotsian10.github.io/Sianlang/'
with urllib.request.urlopen(site, timeout=30) as response:
    html = response.read().decode('utf-8')
assert 'SianLang' in html and 'v0.3.1' in html
print('Pages HTTP 200: ' + site)
base = 'https://github.com/rodotsian10/Sianlang/releases/download/v0.3.1/'
for name in ['SianLang-0.3.1-windows-x64.zip', 'sianlang-vscode-0.3.1.vsix', 'SHA256SUMS.txt']:
    assert base + name in html
    with urllib.request.urlopen(base + name, timeout=30) as response:
        content = response.read()
    expected = (root / 'dist/0.3.1' / name).read_bytes()
    assert hashlib.sha256(content).digest() == hashlib.sha256(expected).digest()
    print('Public download verified: ' + name)

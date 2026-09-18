"""Validate the delivered artifacts in an isolated Unicode directory, without developer PATH."""
import hashlib
import json
import os
from pathlib import Path
import shutil
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
    game_example = portable / 'sianlanggameex1'
    converted = subprocess.run([str(exe), 'Conversion.sian'], cwd=game_example,
                               env=env, capture_output=True, encoding='utf-8', timeout=20)
    assert converted.returncode == 0, converted.stderr
    sprite = (game_example / 'player.rodot').read_bytes()
    assert sprite.startswith((game_example / 'sprite.png').read_bytes())
    assert sprite.endswith(b'RODOT001')
    fresh_game = Path(temp) / 'fresh-game'
    fresh_game.mkdir()
    for name in ['sprite.png', 'game.sian']:
        shutil.copyfile(game_example / name, fresh_game / name)
    game_env = env.copy()
    game_env['SIAN_HEADLESS'] = '1'
    title = subprocess.Popen([str(exe), 'game.sian'], cwd=fresh_game, env=game_env,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        try:
            out, err = title.communicate(timeout=1)
            raise AssertionError(f'Game stopped on the title screen: {title.returncode}: {out} {err}')
        except subprocess.TimeoutExpired:
            title.terminate()
            out, err = title.communicate(timeout=5)
            assert not err, err
    finally:
        if title.poll() is None:
            title.kill()
            title.communicate()
    assert (fresh_game / 'player.rodot').is_file(), 'First run did not create player.rodot'
    source = (fresh_game / 'game.sian').read_text(encoding='utf-8')
    play_source = source.replace('game.start("title",', 'game.start("play",', 1)
    stop_after_frame = '    if key.down("q")\n        game.close()\n\nscene win'
    assert play_source != source and stop_after_frame in play_source
    play_source = play_source.replace(stop_after_frame, '    game.close()\n\nscene win', 1)
    (fresh_game / 'smoke.sian').write_text(play_source, encoding='utf-8')
    played = subprocess.run([str(exe), 'smoke.sian'], cwd=fresh_game, env=game_env,
                            capture_output=True, text=True, timeout=10)
    assert played.returncode == 0, played.stderr
    with ZipFile(portable / f"{meta['name']}-{version}.vsix") as extension:
        assert extension.read('extension/bin/win32-x64/Sianlang.exe') == exe.read_bytes()
        assert extension.read('extension/icons/sianlang-file.svg') == (ROOT / 'Sianlangicon.svg').read_bytes()
        packed = json.loads(extension.read('extension/package.json'))
        assert packed['contributes']['languages'][0]['icon']['dark'] == './icons/sianlang-file.svg'
        snippets = json.loads(extension.read('extension/snippets/sianlang.json'))
        assert len(snippets) == 16
        assert extension.read('extension/LICENSE') == (ROOT / 'LICENSE').read_bytes()
    assert (portable / 'LICENSE').read_bytes() == (ROOT / 'LICENSE').read_bytes()
print('Release checks passed: SHA256, download links, clean-PATH portable execution, bundled runtime and icon.')

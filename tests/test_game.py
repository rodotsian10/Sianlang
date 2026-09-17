"""Exercise the Windows scene loop and binary sprite format."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--exe', required=True)
EXE = Path(parser.parse_args().exe).resolve()

def run(path):
    environment = os.environ.copy()
    environment['SIAN_HEADLESS'] = '1'
    return subprocess.run([str(EXE), str(path)], cwd=ROOT,
                          text=True, capture_output=True, timeout=15, encoding='utf-8', env=environment)

scene = run(ROOT / 'tests/game-scenes.sian')
assert scene.returncode == 0, scene.stderr
assert scene.stdout == 'title\ngame\ngame over\n', scene.stdout

generated = ROOT / 'build/icon.rodot'
generated.unlink(missing_ok=True)
sprite = run(ROOT / 'tests/rodot-smoke.sian')
assert sprite.returncode == 0, sprite.stderr
assert sprite.stdout == 'icon 128 128\n42 8 100\n44 False 128 128\n0\nTrue\nFalse\n44 False 100\n', sprite.stdout
png = (ROOT / 'sianlang-vscode/icon.png').read_bytes()
rodot = generated.read_bytes()
assert rodot.startswith(png)
assert rodot.endswith(b'RODOT001')
duplicate = run(ROOT / 'tests/rodot-smoke.sian')
assert duplicate.returncode != 0 and 'already exists' in duplicate.stderr
assert generated.read_bytes() == rodot
immutable = run(ROOT / 'tests/rodot-immutable.sian')
assert immutable.returncode != 0 and 'Frodot may modify r.data only' in immutable.stderr
bad = ROOT / 'build/bad.rodot'
bad.write_bytes(rodot[:-1] + b'X')
malformed = run(ROOT / 'tests/rodot-malformed.sian')
assert malformed.returncode != 0 and 'invalid rodot metadata trailer' in malformed.stderr

render = run(ROOT / 'tests/rodot-scene.sian')
assert render.returncode == 0, render.stderr
assert render.stdout == '59 20\n', render.stdout

print('Game scenes, temporary Frodot edits, saved metadata, and PNG preservation passed.')

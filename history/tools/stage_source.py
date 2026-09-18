"""Copy an explicit source whitelist into the empty release checkout."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parent.parent
target = root / 'build/github-release'
assert (target / '.git').is_dir()
directories = ['src', 'tests', 'tools', '설명서', '배포', '.github', '.vscode', 'sianlang-vscode', 'docs', 'review/cases']
ignored = shutil.ignore_patterns('*.exe', '*.vsix', '*.obj', '*.pdb', '*.log', '__pycache__', 'node_modules', 'bin')
for directory in directories:
    shutil.copytree(root / directory, target / directory, dirs_exist_ok=True, ignore=ignored)
for name in ['.gitignore', '.gitattributes', 'README.md', 'LICENSE', 'main.c', 'build.ps1', 'CHANGELOG.md',
             '설명서.md', '개발순서.md', '게임기능-계획.md', 'Sianlangicon.svg', 'code.sian',
             'features-demo.sian', 'input-test.sian', 'input-types-test.sian', 'try-catch-test.sian']:
    shutil.copyfile(root / name, target / name)
print('Prepared source checkout (no build artifacts or credentials).')

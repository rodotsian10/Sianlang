import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
results = []
for executable in ['Sianlang.exe', 'SianlangA.exe', 'review/Sianlang-review.exe']:
    for case, data in [('input-test.sian', 'Sian\n'), ('input-types-test.sian', '3\n1.5\ntrue\n'), ('iferror-test.sian', ''), ('code.sian', '50\n')]:
        try:
            p = subprocess.run([str(ROOT / executable), str(ROOT / case)], input=data, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=3)
            results.append(dict(executable=executable, case=case, exit=p.returncode, stdout=p.stdout, stderr=p.stderr))
        except subprocess.TimeoutExpired:
            results.append(dict(executable=executable, case=case, timeout=True))
(ROOT / 'review' / 'existing-results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
for row in results:
    print(json.dumps(row, ensure_ascii=False))

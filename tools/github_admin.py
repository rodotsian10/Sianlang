"""Session helper: use existing Git credentials in memory; never print/store credentials."""
import json
import os
from pathlib import Path
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
REPO = 'rodotsian10/Sianlang'
env = os.environ.copy()
env.update(GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='never')
credentials = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n\n',
                             text=True, capture_output=True, env=env, timeout=20)
fields = dict(line.split('=', 1) for line in credentials.stdout.splitlines() if '=' in line)
token = fields.get('password')
if not token:
    raise SystemExit('No existing GitHub credential available. Sign in with Git Credential Manager first.')

def request(method, endpoint, data=None, binary=False):
    url = endpoint if endpoint.startswith('https://') else f'https://api.github.com/repos/{REPO}{endpoint}'
    if urllib.parse.urlparse(url).hostname not in ('api.github.com', 'uploads.github.com'):
        raise ValueError('Unexpected API host')
    headers = {'Authorization': f'Bearer {token}', 'Accept': 'application/vnd.github+json',
               'User-Agent': 'SianLang-release', 'X-GitHub-Api-Version': '2022-11-28'}
    if data is not None:
        headers['Content-Type'] = 'application/octet-stream' if binary else 'application/json'
    payload = data if binary else (json.dumps(data).encode() if data is not None else None)
    try:
        with urllib.request.urlopen(urllib.request.Request(url, data=payload, headers=headers, method=method), timeout=45) as response:
            body = response.read()
            return json.loads(body) if body else {}
    except urllib.error.HTTPError as error:
        message = json.loads(error.read()).get('message', error.reason)
        raise RuntimeError(f'GitHub {method} {urllib.parse.urlparse(url).path}: HTTP {error.code}: {message}') from None

mode = sys.argv[1]
if mode == 'status':
    repo = request('GET', '')
    print(json.dumps({key: repo.get(key) for key in ['full_name', 'private', 'default_branch', 'permissions', 'size', 'has_pages']}, indent=2))
elif mode == 'publish':
    version = json.loads((ROOT / 'sianlang-vscode/package.json').read_text(encoding='utf-8'))['version']
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    releases = request('GET', '/releases')
    if any(r['tag_name'] == f'v{version}' for r in releases):
        raise SystemExit('Release already exists; refusing to overwrite it.')
    release = request('POST', '/releases', {'tag_name': f'v{version}', 'target_commitish': commit,
        'name': f'SianLang {version} — Windows x64', 'draft': True, 'prerelease': True,
        'body': (ROOT / '배포/release-notes.md').read_text(encoding='utf-8')})
    print(f'Created draft release {release["id"]}')
    dist = ROOT / 'dist' / version
    for name in [f'SianLang-{version}-windows-x64.zip', f'sianlang-vscode-{version}.vsix', 'SHA256SUMS.txt']:
        asset = request('POST', release['upload_url'].split('{')[0] + '?name=' + urllib.parse.quote(name), (dist / name).read_bytes(), binary=True)
        assert asset['size'] == (dist / name).stat().st_size
        print(f'Uploaded {name} ({asset["size"]} bytes)')
    published = request('PATCH', f'/releases/{release["id"]}', {'draft': False})
    print(published['html_url'])
elif mode == 'pages':
    repo = request('GET', '')
    if repo.get('has_pages'):
        page = request('GET', '/pages')
        if page.get('source') != {'branch': 'main', 'path': '/docs'}:
            raise SystemExit('Existing Pages configuration differs; refusing to overwrite it.')
    else:
        page = request('POST', '/pages', {'source': {'branch': 'main', 'path': '/docs'}, 'build_type': 'legacy'})
    request('PATCH', '', {'description': '쉽게 배우는 프로그래밍 언어 · Windows 실행기와 VS Code 확장 · 2D 게임 제작을 목표로 개발 중', 'homepage': 'https://rodotsian10.github.io/Sianlang/'})
    print(json.dumps({key: page.get(key) for key in ['html_url', 'status', 'source']}, indent=2))
elif mode == 'verify':
    version = json.loads((ROOT / 'sianlang-vscode/package.json').read_text(encoding='utf-8'))['version']
    for endpoint in ['/pages', '/pages/builds/latest', '/actions/runs?per_page=3', f'/releases/tags/v{version}']:
        try:
            result = request('GET', endpoint)
            if 'workflow_runs' in result:
                result = [{k: r.get(k) for k in ['name', 'status', 'conclusion', 'html_url']} for r in result['workflow_runs']]
            else:
                result = {k: result[k] for k in ['html_url', 'status', 'error', 'draft', 'prerelease', 'assets'] if k in result}
                if 'assets' in result: result['assets'] = [{k: a[k] for k in ['name', 'size', 'browser_download_url']} for a in result['assets']]
            print(endpoint, json.dumps(result))
        except RuntimeError as error:
            print(error)
else:
    raise SystemExit('Unknown operation')

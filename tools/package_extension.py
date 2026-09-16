"""Build this dependency-free extension as a local VSIX, without publishing it."""
import json
import subprocess
from pathlib import Path
from xml.etree import ElementTree as ET
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parent.parent / 'sianlang-vscode'
meta = json.loads((ROOT / 'package.json').read_text(encoding='utf-8'))
runtime = ROOT.parent / 'build' / 'Sianlang.exe'
version = subprocess.check_output([str(runtime), '--version'], text=True).strip()
if version != f"SianLang {meta['version']}":
    raise SystemExit(f'Runtime/extension version mismatch: {version}')
binary = ROOT / 'bin' / 'win32-x64' / 'Sianlang.exe'
binary.parent.mkdir(parents=True, exist_ok=True)
binary.write_bytes(runtime.read_bytes())
(ROOT / 'icons' / 'sianlang-file.svg').write_bytes((ROOT.parent / 'Sianlangicon.svg').read_bytes())
(ROOT / 'LICENSE').write_bytes((ROOT.parent / 'LICENSE').read_bytes())
VSIX = 'http://schemas.microsoft.com/developer/vsx-schema/2011'
ET.register_namespace('', VSIX)
manifest = ET.Element(f'{{{VSIX}}}PackageManifest', Version='2.0.0')
metadata = ET.SubElement(manifest, 'Metadata')
ET.SubElement(metadata, 'Identity', Language='en-US', Id=meta['name'], Version=meta['version'], Publisher=meta['publisher'])
ET.SubElement(metadata, 'DisplayName').text = meta['displayName']
ET.SubElement(metadata, 'Description').text = meta['description']
ET.SubElement(metadata, 'Categories').text = ','.join(meta['categories'])
ET.SubElement(metadata, 'Tags').text = ','.join(meta['keywords'])
ET.SubElement(metadata, 'Icon').text = 'extension/icon.png'
properties = ET.SubElement(metadata, 'Properties')
for key, value in {
    'Microsoft.VisualStudio.Code.Engine': meta['engines']['vscode'],
    'Microsoft.VisualStudio.Code.ExtensionKind': 'workspace',
    'Microsoft.VisualStudio.Code.ExecutesCode': 'true',
    'Microsoft.VisualStudio.Services.GitHubFlavoredMarkdown': 'true',
}.items():
    ET.SubElement(properties, 'Property', Id=key, Value=value)
installation = ET.SubElement(manifest, 'Installation')
ET.SubElement(installation, 'InstallationTarget', Id='Microsoft.VisualStudio.Code')
ET.SubElement(manifest, 'Dependencies')
assets = ET.SubElement(manifest, 'Assets')
for kind, path in [('Microsoft.VisualStudio.Code.Manifest', 'extension/package.json'),
                   ('Microsoft.VisualStudio.Services.Content.Details', 'extension/README.md'),
                   ('Microsoft.VisualStudio.Services.Content.License', 'extension/LICENSE'),
                   ('Microsoft.VisualStudio.Services.Icons.Default', 'extension/icon.png')]:
    ET.SubElement(assets, 'Asset', Type=kind, Path=path, Addressable='true')

content_types = ET.Element('Types', xmlns='http://schemas.openxmlformats.org/package/2006/content-types')
for extension, content_type in [('json', 'application/json'), ('js', 'application/javascript'),
                                 ('md', 'text/markdown'), ('png', 'image/png'), ('svg', 'image/svg+xml'),
                                 ('exe', 'application/octet-stream'), ('vsixmanifest', 'text/xml')]:
    ET.SubElement(content_types, 'Default', Extension=extension, ContentType=content_type)

files = ['package.json', 'extension.js', 'language-configuration.json', 'README.md', 'icon.png',
         'syntaxes/sianlang.tmLanguage.json', 'icons/sianlang-file.svg', 'icons/sianlang-icon-theme.json',
         'docs/grammar.md', 'snippets/sianlang.json', 'bin/win32-x64/Sianlang.exe', 'LICENSE']
(ROOT / 'docs').mkdir(exist_ok=True)
(ROOT / 'docs' / 'grammar.md').write_bytes((ROOT.parent / '설명서' / '시안랭-문법규칙서.md').read_bytes())
output = ROOT / f"{meta['name']}-{meta['version']}.vsix"
with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
    archive.writestr('extension.vsixmanifest', ET.tostring(manifest, encoding='utf-8', xml_declaration=True))
    archive.writestr('[Content_Types].xml', ET.tostring(content_types, encoding='utf-8', xml_declaration=True))
    for file in files:
        archive.write(ROOT / file, 'extension/' + file)
with ZipFile(output) as archive:
    assert archive.testzip() is None
    parsed = ET.fromstring(archive.read('extension.vsixmanifest'))
    assert parsed.find(f'{{{VSIX}}}Metadata/{{{VSIX}}}Identity').attrib['Version'] == meta['version']
    assert json.loads(archive.read('extension/package.json')) == meta
    for file in files:
        assert archive.read('extension/' + file) == (ROOT / file).read_bytes()
print(f'Built and verified {output.name}', flush=True)

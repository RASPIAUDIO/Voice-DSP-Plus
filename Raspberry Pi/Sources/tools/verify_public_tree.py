"""Verify this publication's links, source/binary separation and package hashes."""
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import unquote
import zipfile

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT/'Raspberry Pi/Sources'
FIRMWARE = ROOT/'Raspberry Pi/Firmware'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    assert {p.name for p in ROOT.iterdir() if p.is_dir() and not p.name.startswith('.')} == {
        'Raspberry Pi', 'USB', 'Home Assistant'}
    links = 0
    for file in ROOT.rglob('*.md'):
        if '.git' in file.parts:
            continue
        for target in re.findall(r'\]\(([^)]+)\)', file.read_text(encoding='utf-8')):
            if target.startswith(('http:', 'https:', '#', 'mailto:')):
                continue
            dest = (file.parent/unquote(target.split('#', 1)[0])).resolve()
            assert dest.is_relative_to(ROOT) and dest.exists(), (file, target)
            links += 1
    # No machine credentials, private repository paths or local identities in text.
    forbidden = re.compile(r'192\.168\.\d+\.\d+|C:[/\\]Users[/\\]|'
                           r'RASPIAUDIOadmin|VoiceDSP_V2_internal|RoscoWifi|'
                           r'github_pat_[A-Za-z0-9_]+|gh[pousr]_[A-Za-z0-9]{20,}|'
                           r'-----BEGIN (?:OPENSSH|RSA|EC) PRIVATE KEY-----')
    for file in ROOT.rglob('*'):
        if not file.is_file() or '.git' in file.parts or file == Path(__file__):
            continue
        if file.suffix in ('.bin', '.xe', '.zip', '.run', '.png', '.jpg', '.pyc'):
            continue
        assert not forbidden.search(file.read_text(encoding='utf-8')), file
    archive = FIRMWARE/'voicedsp-v2-pi48-r8-kit.zip'
    installer = FIRMWARE/'voicedsp-v2-pi48-r8.run'
    for file in (archive, installer):
        assert sha(file.read_bytes()) == file.with_suffix('.sha256').read_text().split()[0]
    with zipfile.ZipFile(archive) as bundle:
        assert bundle.testzip() is None
        manifest = json.loads(bundle.read('manifest.json'))
        assert manifest['hardware_installable']
        assert manifest['files'].keys() == set(bundle.namelist()) - {'manifest.json'}
        for name, expected in manifest['files'].items():
            data = bundle.read(name)
            assert sha(data) == expected, name
            original = FIRMWARE/Path(name).name if name.startswith('firmware/') else SOURCE/name
            assert data == original.read_bytes(), name
        assert 'licenses/PAJ-LICENSE.txt' in manifest['files']
        assert 'LICENSE' in manifest['files']
        assert 'firmware/LICENSE.xmos-package.rst' in manifest['files']
    header, payload = installer.read_bytes().split(b'\n__VOICEDSP_ZIP__\n', 1)
    assert payload == archive.read_bytes()
    assert sha(payload).encode() in header
    print(f'PASS: {links} links; public text scan; complete source/firmware parity; ZIP and installer hashes')


if __name__ == '__main__':
    main()

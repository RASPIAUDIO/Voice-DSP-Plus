"""Offline integrity checks for both public USB and Home Assistant packages."""
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[3]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    checked = 0
    for mode in ('USB', 'Home Assistant'):
        firmware = ROOT/mode/'Firmware'
        release = json.loads((firmware/'release.json').read_text())
        sums = dict(line.split('  ', 1)[::-1] for line in (firmware/'SHA256SUMS').read_text().splitlines())
        for name, info in release['files'].items():
            data = (firmware/name).read_bytes()
            assert len(data) == info['bytes'], name
            assert sha(data) == info['sha256'] == sums[name], name
            checked += 1
        delta = ROOT/mode/'Sources/XMOS'
        patches = json.loads((delta/'changes.json').read_text())
        expected = json.loads((delta/'SOURCE_SHA256.json').read_text())
        actual = {k: v['sha256'] for k, v in patches['patches'].items()}
        actual.update(patches['new_files'])
        assert actual == expected, mode + ' source inventory'
        for name, digest in patches['new_files'].items():
            assert sha((delta/'overlay'/name).read_bytes()) == digest, name
    ha = ROOT/'Home Assistant'
    version = json.loads((ha/'Firmware/release.json').read_text())['version']
    factory = (ha/f'Firmware/voicedsp-v2-{version}.factory.bin').read_bytes()
    ota = (ha/f'Firmware/voicedsp-v2-{version}.ota.bin').read_bytes()
    assert factory[0x10000:] == ota, 'Factory/OTA mismatch'
    xmos = (ha/'Firmware/xmos-i2s48.spi_boot.bin').read_bytes()
    assert (ha/'Sources/ESPHome/components/voicedsp_v2/xmos_spi.bin').read_bytes() == xmos
    assert xmos in ota, 'XMOS image is not embedded in OTA'
    manifest = json.loads((ha/'Firmware/manifest.json').read_text())
    assert manifest['version'].split('-')[0] == version
    part = manifest['builds'][0]['parts'][0]
    assert part == {'path': f'voicedsp-v2-{version}.factory.bin', 'offset': 0}
    # Check local Markdown links in the new guides, without fetching network links.
    documents = [ROOT/'README.md', ROOT/'NOTICE.md', ROOT/'UPSTREAM.md']
    for mode in ('USB', 'Home Assistant'):
        documents.extend((ROOT/mode).rglob('*.md'))
    for doc in documents:
        for link in re.findall(r'\]\(([^)]+)\)', doc.read_text(encoding='utf-8')):
            if '://' in link or link.startswith(('#', 'mailto:')):
                continue
            path = unquote(link.split('#')[0])
            assert (doc.parent/path).exists(), f'{doc}: broken link {link}'
    print(f'PASS: {checked} firmware hashes, source inventories, embedded XMOS, factory/OTA, manifest and guide links')


if __name__ == '__main__':
    main()

"""Apply RASPIAUDIO source changes to an owner-supplied licensed XMOS SDK."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_name(name):
    name = name.removeprefix('./')
    p = PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts or '\\' in name or ':' in name:
        raise ValueError('Unsafe archive path: ' + name)
    return p


def prepare(sdk, delta, output):
    manifest = json.loads((delta/'changes.json').read_text())
    if sha(sdk.read_bytes()) != manifest['sdk_archive_sha256']:
        raise ValueError('Wrong SDK archive; use the exact version/hash in changes.json')
    if output.exists():
        raise ValueError('Output must not exist; never modify the source SDK in place')
    with tarfile.open(sdk) as archive:
        members = archive.getmembers()
        for item in members:
            safe_name(item.name)
            if not (item.isdir() or item.isfile()):
                raise ValueError('Archive links/special files are not accepted')
        output.mkdir(parents=True)
        for item in members:
            dest = output/safe_name(item.name)
            if item.isdir():
                dest.mkdir(parents=True, exist_ok=True)
            else:
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(archive.extractfile(item).read())
    for name, patch in manifest['patches'].items():
        dest = output/safe_name(name)
        text = dest.read_text(encoding='utf-8')
        if sha(text.encode()) != patch['base_sha256']:
            raise ValueError('Unexpected source: ' + name)
        lines = text.splitlines(keepends=True)
        for edit in reversed(patch['edits']):
            lines[edit['start']:edit['end']] = edit['text'].splitlines(keepends=True)
        data = ''.join(lines).encode()
        if sha(data) != patch['sha256']:
            raise ValueError('Patch verification failed: ' + name)
        dest.write_bytes(data)
    for name, digest in manifest['new_files'].items():
        relative = safe_name(name)
        data = (delta/'overlay'/relative).read_bytes()
        if sha(data) != digest:
            raise ValueError('Overlay verification failed: ' + name)
        dest = output/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
    print('Prepared source. Target:', manifest['target'])
    print('No build, device discovery or flash performed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--delta', type=Path, default=Path(__file__).parent/'XMOS')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.sdk.resolve(), args.delta.resolve(), args.output.resolve())

"""Assemble a checked Pi kit from this public tree; no hardware or network access."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent
IMAGES = {
    'xmos-i2s48.spi_boot.bin': '152334171334c9608783985ee83ed4e3daad94385dd220bc4a5884b4eaa7dc50',
    'xmos-i2s48.xe': 'ee772562ce81b87e535d09175a6f5ed8cd15655983843b03af4e2bb331093d12',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(output, firmware_dir=None):
    output = output.resolve()
    if output.exists() or output.is_relative_to(ROOT):
        raise ValueError('Use a new destination outside Sources')
    if firmware_dir is not None:
        firmware_dir = firmware_dir.resolve()
        for name, expected in IMAGES.items():
            image = firmware_dir / name
            if not image.is_file() or digest(image) != expected:
                raise ValueError('Missing or incorrect paired firmware: ' + name)
        if not (firmware_dir / 'LICENSE.xmos-package.rst').is_file():
            raise ValueError('Keep the XMOS package licence with the firmware')
    files = []
    for source in ROOT.rglob('*'):
        relative = source.relative_to(ROOT)
        if relative.as_posix() == 'manifest.json' or relative.parts[0] == 'firmware':
            continue
        if any(p in ('build', '__pycache__', '.git') for p in relative.parts):
            continue
        if source.is_symlink():
            raise ValueError('Symlink not permitted in source kit: ' + str(relative))
        if source.is_file() and source.suffix != '.pyc':
            files.append((source, relative))
    output.mkdir(parents=True)
    for source, relative in files:
        dest = output / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
    if firmware_dir is not None:
        (output / 'firmware').mkdir()
        for name in (*IMAGES, 'LICENSE.xmos-package.rst'):
            shutil.copyfile(firmware_dir / name, output / 'firmware' / name)
    policy = json.loads((output / 'accepted-aec.json').read_text())
    manifest = {
        'rate': 48000, 'transport': 'i2s', 'slots': 2, 'slot_bits': 32,
        'kit_version': policy['kit_version'], 'host_i2c_owner': 'raspberry_pi',
        'hardware_installable': firmware_dir is not None,
        'xmos_spi_sha256': IMAGES['xmos-i2s48.spi_boot.bin'],
        'files': {p.relative_to(output).as_posix(): digest(p)
                  for p in sorted(output.rglob('*')) if p.is_file()},
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--archive', type=Path, help='Optional fresh ZIP outside the output directory')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--firmware-dir', type=Path, default=ROOT.parent / 'Firmware')
    group.add_argument('--without-firmware', action='store_true')
    args = parser.parse_args()
    if args.archive:
        archive = args.archive.resolve()
        if (archive.exists() or archive.with_suffix('.sha256').exists()
                or archive.is_relative_to(args.output.resolve())):
            parser.error('Archive and checksum must be new and outside the output directory')
    manifest = prepare(args.output, None if args.without_firmware else args.firmware_dir)
    if args.archive:
        archive.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as bundle:
            for name in sorted([*manifest['files'], 'manifest.json']):
                bundle.write(args.output/name, name)
        archive.with_suffix('.sha256').write_bytes((digest(archive)+'  '+archive.name+'\n').encode())
    print(json.dumps({'output': str(args.output), 'files': len(manifest['files']),
                      'hardware_installable': manifest['hardware_installable']}))

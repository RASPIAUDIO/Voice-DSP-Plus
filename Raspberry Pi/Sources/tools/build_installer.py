"""Build a self-extracting installer; no network, secrets or hardware access."""
import argparse
import hashlib
from pathlib import Path
import zipfile

HEADER = '''#!/bin/sh
set -eu
if [ "$(id -u)" != 0 ]; then
  echo "Use: sudo bash $0 --confirm-v2-exclusive --reboot" >&2
  exit 1
fi
umask 077
work=$(mktemp -d /var/tmp/voicedsp-install.XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT HUP INT TERM
line=$(awk '/^__VOICEDSP_ZIP__$/ { print NR + 1; exit }' "$0")
tail -n +"$line" "$0" > "$work/kit.zip"
printf '%s  %s\\n' '__SHA__' "$work/kit.zip" | sha256sum -c -
python3 - "$work" <<'PY'
from pathlib import Path
import stat, sys, zipfile
root = Path(sys.argv[1]) / 'kit'
with zipfile.ZipFile(root.parent / 'kit.zip') as z:
    for info in z.infolist():
        dest = (root / info.filename).resolve()
        if not dest.is_relative_to(root.resolve()) or stat.S_ISLNK(info.external_attr >> 16):
            raise SystemExit('Unsafe archive path')
    z.extractall(root)
root.chmod(0o755)
for path in root.rglob('*'):
    path.chmod(0o755 if path.is_dir() else 0o644)
PY
export PYTHONDONTWRITEBYTECODE=1
python3 "$work/kit/scripts/one_command.py" "$@"
exit 0
__VOICEDSP_ZIP__
'''


def build(archive, output):
    if output.exists() or output.with_suffix('.sha256').exists():
        raise ValueError('Use a fresh installer filename')
    data = archive.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    if archive.with_suffix('.sha256').read_text().split()[0] != sha:
        raise ValueError('ZIP checksum mismatch')
    with zipfile.ZipFile(archive) as z:
        if z.testzip() or 'scripts/one_command.py' not in z.namelist():
            raise ValueError('Not a complete one-command kit')
    output.write_bytes(HEADER.replace('__SHA__', sha).encode('ascii') + data)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.sha256').write_bytes((digest + '  ' + output.name + '\n').encode('ascii'))
    print(f'{digest}  {output}')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--archive', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    build(args.archive, args.output)

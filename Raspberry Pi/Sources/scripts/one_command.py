"""Fresh-Pi installer, followed by a bounded post-reboot setup service."""
import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
import install
from configure import configure
from discover import discover

ROOT = Path(__file__).resolve().parents[1]
PENDING = install.CONFIG / 'setup-pending.json'
SETUP = install.SETUP_UNIT
NAME = 'voicedsp-v2-48k.service'
SETUP_NAME = 'voicedsp-v2-48k-setup.service'
BOOT_ID = Path('/proc/sys/kernel/random/boot_id')


def run(args):
    print('+ ' + ' '.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True)


def save(path, value):
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(json.dumps(value, indent=2) + '\n')
    tmp.replace(path)


def verify_payload(source):
    manifest = json.loads((source / 'manifest.json').read_text())
    if manifest.get('hardware_installable') is False:
        raise ValueError('Source-only kit: prepare again with the separately obtained matching firmware')
    for name, expected in manifest['files'].items():
        file = (source / name).resolve()
        if not file.is_relative_to(source.resolve()) or install.digest(file) != expected:
            raise ValueError('Kit hash mismatch: ' + name)


def require_fresh(root):
    for rel in (install.PREFIX, install.CONFIG, install.UNIT, SETUP, install.OVERLAY):
        if (root / rel).exists():
            raise FileExistsError('Existing installation preserved: ' + str(root / rel))
    boot = (root / install.BOOT).read_text()
    if 'voicedsp-v2-48k' in boot or re.search(
            r'^\s*dtoverlay=.*(?:i2s|audio|voice|hifi|wm8960|adau)', boot, re.M | re.I):
        raise ValueError('Conflicting audio overlay: review manually; nothing removed')


def checked_user(user):
    import pwd
    if not user or not re.fullmatch(r'[a-z_][a-z0-9_-]*', user):
        raise ValueError('Run with sudo from a normal user, or supply --user USER')
    if pwd.getpwnam(user).pw_uid == 0:
        raise ValueError('Service user must not be root')
    return user


def stage_setup(root, user, boot_id, desktop=False):
    state = json.loads((root / install.STATE).read_text())
    unit = '''[Unit]
Description=Finish Voice DSP+ V2 installation after overlay reboot
After=sound.target
ConditionPathExists=/etc/voicedsp-v2-48k/setup-pending.json

[Service]
Type=oneshot
Environment=PYTHONDONTWRITEBYTECODE=1
ExecStart=/usr/bin/python3 /opt/voicedsp-v2-48k/scripts/one_command.py --finish
TimeoutStartSec=180

[Install]
WantedBy=multi-user.target
'''
    (root / SETUP).write_text(unit)
    pending = {'user': user, 'previous_boot': boot_id}
    if desktop:
        pending['desktop'] = True
    save(root / PENDING, pending)
    for rel in (SETUP, PENDING):
        state['files'][rel.as_posix()] = install.digest(root / rel)
    state['one_command'] = {'status': 'pending-reboot', 'user': user}
    save(root / install.STATE, state)


def verify_managed(root):
    state = json.loads((root / install.STATE).read_text())
    for name, sha in state['files'].items():
        file = (root / name).resolve()
        if not file.is_relative_to(root.resolve()) or install.digest(file) != sha:
            raise ValueError('Managed file modified; refusing automatic setup: ' + name)
    return state


def configure_detected(root, audio):
    state = verify_managed(root)
    with tempfile.TemporaryDirectory() as tmp:
        staged = Path(tmp) / 'config'
        configure(staged, audio['card'], audio['playback_device'], audio['capture_device'])
        before = {}
        try:
            for file in staged.iterdir():
                rel = install.CONFIG / file.name
                if rel.as_posix() not in state['files']:
                    raise ValueError('Unmanaged generated config: ' + str(rel))
                target = root / rel
                before[target] = target.read_bytes()
                target.write_bytes(file.read_bytes())
                state['files'][rel.as_posix()] = install.digest(target)
            save(root / install.STATE, state)
        except BaseException:
            for target, data in before.items():
                target.write_bytes(data)
            raise


def finish(root=Path('/'), runner=run, get_audio=discover, boot_id=None, sleep=time.sleep):
    if not (root / PENDING).exists():
        print('No pending setup; existing settings left unchanged.')
        return
    state = verify_managed(root)
    if state.get('one_command', {}).get('status') == 'verified':
        (root / PENDING).unlink()
        print('Previously verified setup finalized; audio settings unchanged.')
        return
    pending = json.loads((root / PENDING).read_text())
    current = boot_id if boot_id is not None else BOOT_ID.read_text().strip()
    if current == pending['previous_boot']:
        raise ValueError('Reboot required before finalization; no audio started')
    try:
        for attempt in range(30):
            try:
                audio = get_audio()
                break
            except (OSError, ValueError):
                if attempt == 29:
                    raise
                sleep(1)
        configure_detected(root, audio)
        # Start before enabling: an unverified service must not run at future boots.
        runner(['systemctl', 'start', NAME])
        runner(['runuser', '-u', pending['user'], '--', '/usr/bin/python3',
                root / install.PREFIX / 'scripts/verify_installation.py', '--startup', '--timeout', '90'])
        runner(['systemctl', 'enable', NAME])
        if pending.get('desktop'):
            runner(['/usr/bin/python3', root / install.PREFIX / 'scripts/desktop.py',
                    'install', '--user', pending['user']])
        runner(['systemctl', 'disable', SETUP_NAME])
        state = json.loads((root / install.STATE).read_text())
        state['service_enabled'] = True
        state['one_command'] = {'status': 'verified', 'user': pending['user'],
                                'boot_id': current, 'audio': audio}
        state['files'].pop(PENDING.as_posix())
        save(root / install.STATE, state)
        (root / PENDING).unlink()
        print('SUCCESS: accepted AEC verified. No test sound played. ' +
              ('Desktop input/output enabled; internal speaker, initial desktop volume 25%.'
               if pending.get('desktop') else 'Output OFF, -30 dB.'))
    except BaseException:
        runner(['systemctl', 'disable', '--now', NAME])
        print('Setup FAILED. Audio service disabled. Inspect journalctl -u ' + SETUP_NAME,
              file=sys.stderr)
        raise


def initial(user, reboot, desktop=False):
    root = Path('/')
    model = Path('/proc/device-tree/model').read_text().strip('\0')
    if 'Raspberry Pi 4 Model B' not in model or platform.machine() != 'aarch64':
        raise ValueError('One-command release supports Pi4 Model B with 64-bit OS only')
    require_fresh(root)
    verify_payload(ROOT)
    user = checked_user(user)
    run(['apt-get', 'update'])
    run(['apt-get', 'install', '-y', 'build-essential', 'cmake', 'python3', 'alsa-utils',
         'device-tree-compiler', 'psmisc', 'python3-spidev'])
    if desktop:
        run(['apt-get', 'install', '-y', 'pipewire', 'pipewire-pulse', 'wireplumber',
             'pipewire-alsa', 'pulseaudio-utils'])
    if not shutil.which('pinctrl'):
        run(['apt-get', 'install', '-y', 'rpi-utils'])
    if not shutil.which('pinctrl'):
        raise ValueError('pinctrl unavailable; no overlay or firmware installed')
    for group in ('audio', 'i2c', 'spi', 'gpio'):
        run(['groupadd', '-f', group])
    run(['usermod', '-a', '-G', 'audio,i2c,spi,gpio', user])
    run(['cmake', '-S', ROOT, '-B', ROOT / 'build', '-DCMAKE_BUILD_TYPE=Release'])
    run(['cmake', '--build', ROOT / 'build', '-j', '4'])
    run(['ctest', '--test-dir', ROOT / 'build', '--output-on-failure'])
    run(['/usr/bin/python3', '-m', 'unittest', 'discover', '-s', ROOT / 'tests', '-p', 'test_*.py', '-v'])
    install.install(root, ROOT, user, '/dev/i2c-1', 'I2S', 1, 0, True, True)
    stage_setup(root, user, BOOT_ID.read_text().strip(), desktop)
    run(['systemctl', 'daemon-reload'])
    run(['systemctl', 'enable', SETUP_NAME])
    print('Installed. After reboot: ALSA discovery, automatic RAM boot and exact AEC check.\n'
          'Follow progress: journalctl -u ' + SETUP_NAME + ' -b --no-pager', flush=True)
    if reboot:
        run(['systemctl', 'reboot', '--no-block'])
    else:
        print('Run sudo reboot to finish. No audio started before reboot.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--user', default=os.environ.get('SUDO_USER'))
    p.add_argument('--confirm-v2-exclusive', action='store_true')
    p.add_argument('--reboot', action='store_true', help='Reboot automatically after successful preparation')
    p.add_argument('--desktop', action='store_true', help='Default desktop speaker and processed microphone (PipeWire)')
    p.add_argument('--finish', action='store_true', help=argparse.SUPPRESS)
    a = p.parse_args()
    if os.geteuid() != 0:
        p.error('Run through sudo from your normal Pi account')
    # The .run extraction directory is private; installed service files must be readable.
    os.umask(0o022)
    if a.finish:
        finish()
    elif not a.confirm_v2_exclusive:
        p.error('Requires --confirm-v2-exclusive: V2 on Pi4, ZeroCore removed, no competing bus owner')
    else:
        initial(a.user, a.reboot, a.desktop)


if __name__ == '__main__':
    main()

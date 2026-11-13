"""Optional Raspberry Pi Desktop bridge; no firmware or AEC parameter writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

PREFIX = Path('/opt/voicedsp-v2-48k')
STATE = Path('/var/lib/voicedsp-desktop/install.json')
SCRIPT = Path('/opt/voicedsp-desktop/desktop.py')
UNIT_NAME = 'voicedsp-desktop.service'
SINK = 'voicedsp_speaker'
SOURCE = 'voicedsp_microphone'
SOCKET = '/run/voicedsp-v2-48k/control.sock'
CARD = 'alsa_card.platform-soc_sound'
DATA = Path(__file__).with_name('desktop-data')


def run(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def fragment(text):
    include = '</usr/share/alsa/alsa.conf>'
    if text.count(include) != 1:
        raise ValueError('Expected one base ALSA include')
    # Never recursively include alsa.conf from inside its conf.d search path.
    return '# Managed by Voice DSP+ desktop installer\n' + text.replace(include, '')


def unit():
    return '''[Unit]
Description=Voice DSP+ desktop speaker and processed microphone
Wants=pipewire-pulse.service wireplumber.service
After=pipewire-pulse.service wireplumber.service
PartOf=pipewire-pulse.service wireplumber.service
StartLimitIntervalSec=0

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/bin/python3 /opt/voicedsp-desktop/desktop.py activate
Restart=on-failure
RestartSec=10
TimeoutStartSec=150

[Install]
WantedBy=default.target
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def install(user, start=True):
    import pwd
    if os.geteuid() != 0:
        raise ValueError('Run install with sudo --user YOUR_DESKTOP_USER')
    account = pwd.getpwnam(user)
    if not account.pw_uid:
        raise ValueError('Desktop user must not be root')
    home = Path(account.pw_dir)
    service = Path('/etc/systemd/system/voicedsp-v2-48k.service').read_text()
    if '\nUser=' + user + '\n' not in service:
        raise ValueError('Desktop and controller must use the same user (ALSA IPC ownership)')
    files = {
        Path('/etc/alsa/conf.d/99-voicedsp-v2.conf'): fragment(
            Path('/etc/voicedsp-v2-48k/voicedsp-v2.conf').read_text()),
        Path('/etc/wireplumber/wireplumber.conf.d/90-voicedsp-v2.conf'):
            (DATA / '90-voicedsp-v2.conf').read_text(),
        Path('/usr/share/alsa-card-profile/mixer/profile-sets/voicedsp-v2.conf'):
            (DATA / 'voicedsp-v2.conf').read_text(),
        Path('/usr/share/applications/voicedsp-output.desktop'):
            (DATA / 'voicedsp-output.desktop').read_text(),
        home / '.config/autostart/voicedsp-output.desktop':
            (DATA / 'voicedsp-output-autostart.desktop').read_text(),
        SCRIPT.with_name('output_selector.py'):
            Path(__file__).with_name('output_selector.py').read_text(),
        home / '.config/systemd/user' / UNIT_NAME: unit(),
        home / '.config/systemd/user/wireplumber.service.d/50-voicedsp.conf': '''[Service]
# Native ACP probing must not race the SPI boot / ALSA clock keeper.
ExecStartPre=/usr/bin/python3 /opt/voicedsp-desktop/desktop.py wait-core
TimeoutStartSec=150
''',
        SCRIPT: Path(__file__).read_text(),
    }
    for asset in DATA.iterdir():
        if asset.is_file():
            files[SCRIPT.parent / 'desktop-data' / asset.name] = asset.read_text()
    old = json.loads(STATE.read_text()) if STATE.exists() else {}
    if old and old['user'] != user:
        raise ValueError('Desktop bridge already belongs to another user')
    for path in files:
        if path.exists() and old.get('files', {}).get(str(path)) != digest(path):
            raise ValueError('Existing unmanaged/modified file preserved: ' + str(path))
    base = ['runuser', '-u', user, '--', 'env',
            'XDG_RUNTIME_DIR=/run/user/' + str(account.pw_uid),
            'DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/' + str(account.pw_uid) + '/bus']
    snapshot = {}
    if start and Path('/run/user/' + str(account.pw_uid) + '/bus').exists():
        for kind, name in (('sink', SINK), ('source', SOURCE)):
            entries = json.loads(run(*base, 'pactl', '-f', 'json', 'list', kind + 's'))
            match = next((entry for entry in entries if entry['name'] == name), None)
            if match:
                snapshot[kind] = match
    run('apt-get', 'install', '-y', 'pipewire-alsa', 'pulseaudio-utils',
        'python3-gi', 'gir1.2-gtk-3.0', 'gir1.2-ayatanaappindicator3-0.1')
    for path, text in files.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        path.chmod(0o644)
        if path.is_relative_to(home):
            for directory in (home / '.config', home / '.config/systemd', path.parent):
                os.chown(directory, account.pw_uid, account.pw_gid)
            os.chown(path, account.pw_uid, account.pw_gid)
    STATE.parent.mkdir(parents=True, exist_ok=True)
    STATE.write_text(json.dumps({'user': user, 'files': {str(p): digest(p) for p in files}}, indent=2) + '\n')
    # Enable without needing an active graphical session or user D-Bus.
    wants = home / '.config/systemd/user/default.target.wants'
    wants.mkdir(parents=True, exist_ok=True)
    os.chown(wants, account.pw_uid, account.pw_gid)
    link = wants / UNIT_NAME
    if not link.exists():
        link.symlink_to('../' + UNIT_NAME)
    if start and Path('/run/user/' + str(account.pw_uid) + '/bus').exists():
        run(*base, 'systemctl', '--user', 'stop', UNIT_NAME)
        run(*base, 'systemctl', '--user', 'daemon-reload')
        run(*base, 'systemctl', '--user', 'restart', 'wireplumber', 'pipewire-pulse')
        run(*base, 'systemctl', '--user', 'restart', UNIT_NAME)
        for kind, entry in snapshot.items():
            run(*base, 'pactl', 'set-' + kind + '-volume', entry['name'],
                *[str(v['value']) for v in entry['volume'].values()])
            run(*base, 'pactl', 'set-' + kind + '-mute', entry['name'], str(int(entry['mute'])))
        # A panel started before the old cardless endpoints were replaced can
        # retain hidden buttons. Only restart the stock, supervised Pi panel.
        processes = run('ps', '-u', user, '-o', 'args=')
        if '/usr/bin/lwrespawn /usr/bin/wf-panel-pi' in processes:
            subprocess.run(['pkill', '-TERM', '-u', user, '-x', 'wf-panel-pi'], check=False)
    print('Desktop card and output selector installed for ' + user + '; AEC unchanged.')


def activate_card():
    cards = json.loads(run('pactl', '-f', 'json', 'list', 'cards'))
    card = next((c for c in cards if c['name'] == CARD), None)
    if card is None or 'voicedsp-duplex' not in card['profiles']:
        raise RuntimeError('Voice DSP+ desktop card not ready')
    if card['active_profile'] != 'voicedsp-duplex':
        run('pactl', 'set-card-profile', CARD, 'voicedsp-duplex')
    for kind, name in (('sink', SINK), ('source', SOURCE)):
        nodes = json.loads(run('pactl', '-f', 'json', 'list', kind + 's'))
        node = next((n for n in nodes if n['name'] == name), None)
        if not node or not node.get('ports') or 'alsa.card' not in node['properties']:
            raise RuntimeError('Desktop ' + kind + ' has no native card/port association')


def wait_core(strict=True, timeout=120):
    sys.path.insert(0, str(PREFIX / 'scripts'))
    from control import request
    from verify_installation import differences
    policy = json.loads((PREFIX / 'accepted-aec.json').read_text())
    deadline = time.monotonic() + timeout
    while True:
        try:
            status = request(SOCKET, 'status')
            errors = differences(status, policy)
            if errors:
                raise RuntimeError('AEC not ready: ' + ', '.join(errors))
            print('Voice DSP+ core ready before desktop probing', flush=True)
            return status
        except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError):
            if time.monotonic() >= deadline:
                if strict:
                    raise
                # Do not prevent unrelated sound cards from working if V2 fails.
                print('Voice DSP+ unavailable; allowing other desktop devices', flush=True)
                return None
            time.sleep(2)


def activate():
    sys.path.insert(0, str(PREFIX / 'scripts'))
    from control import request
    status = wait_core()
    deadline = time.monotonic() + 15
    while True:
        try:
            activate_card()
            break
        except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(1)
    # The ALSA adapter accepts volume only after its first negotiated format.
    # Prime with digital silence, never a test tone or music.
    silence = subprocess.Popen(['pacat', '--playback', '--device=' + SINK,
                                '--rate=48000', '--channels=2', '--format=s16le', '/dev/zero'])
    try:
        time.sleep(3)
        if silence.poll() is not None:
            raise RuntimeError('Desktop ALSA sink could not play digital silence')
    finally:
        silence.terminate()
        silence.wait(timeout=5)
    state_dir = Path.home() / '.local/state/voicedsp-desktop'
    state_dir.mkdir(parents=True, exist_ok=True)
    marker = state_dir / 'defaults.json'
    if not marker.exists():
        previous = {'sink': run('pactl', 'get-default-sink'),
                    'source': run('pactl', 'get-default-source')}
        run('pactl', 'set-sink-volume', SINK, '25%')
        run('pactl', 'set-source-volume', SOURCE, '100%')
        run('pactl', 'set-default-sink', SINK)
        run('pactl', 'set-default-source', SOURCE)
        time.sleep(1)
        sinks = json.loads(run('pactl', '-f', 'json', 'list', 'sinks'))
        current = next(s for s in sinks if s['name'] == SINK)
        if any(v['value'] != 16384 for v in current['volume'].values()):
            raise RuntimeError('Initial desktop volume was not retained; output left unchanged')
        marker.write_text(json.dumps(previous, indent=2) + '\n')
    # Activate the safe built-in route only when the core started OFF. Never
    # replace an explicitly chosen external/full route during a desktop restart.
    if status['requested_route'] == 1:
        request(SOCKET, 'volume -30')
        request(SOCKET, 'output internal')
        request(SOCKET, 'volume 0')
    print(json.dumps({'sink': run('pactl', 'get-default-sink'),
                      'source': run('pactl', 'get-default-source'),
                      'controller': request(SOCKET, 'status')}, indent=2))


def remove():
    if os.geteuid() != 0:
        raise ValueError('Run removal with sudo')
    import pwd
    state = json.loads(STATE.read_text())
    account = pwd.getpwnam(state['user'])
    for name, expected in state['files'].items():
        if digest(Path(name)) != expected:
            raise ValueError('Modified file preserved: ' + name)
    base = ['runuser', '-u', state['user'], '--', 'env',
            'XDG_RUNTIME_DIR=/run/user/' + str(account.pw_uid)]
    if Path('/run/user/' + str(account.pw_uid) + '/bus').exists():
        run(*base, 'systemctl', '--user', 'stop', UNIT_NAME)
    (Path(account.pw_dir) / '.config/systemd/user/default.target.wants' / UNIT_NAME).unlink(missing_ok=True)
    for name in state['files']:
        Path(name).unlink()
    STATE.unlink()
    if Path('/run/user/' + str(account.pw_uid) + '/bus').exists():
        run(*base, 'systemctl', '--user', 'daemon-reload')
        run(*base, 'systemctl', '--user', 'restart', 'wireplumber', 'pipewire-pulse')
        time.sleep(3)
        marker = Path(account.pw_dir) / '.local/state/voicedsp-desktop/defaults.json'
        if marker.exists():
            previous = json.loads(marker.read_text())
            for kind in ('sink', 'source'):
                names = [row.split('\t')[1] for row in
                         run(*base, 'pactl', 'list', 'short', kind + 's').splitlines()]
                if previous.get(kind) in names:
                    run(*base, 'pactl', 'set-default-' + kind, previous[kind])
            marker.unlink()
    print('Removed desktop bridge only; core firmware and AEC unchanged.')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('install', 'activate', 'wait-core', 'remove'))
    p.add_argument('--user', default=os.environ.get('SUDO_USER'))
    p.add_argument('--no-start', action='store_true')
    a = p.parse_args()
    if a.action == 'install':
        install(a.user, not a.no_start)
    elif a.action == 'activate':
        activate()
    elif a.action == 'wait-core':
        wait_core(strict=False)
    else:
        remove()

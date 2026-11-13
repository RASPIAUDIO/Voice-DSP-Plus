"""Install the Pi48 kit, without starting audio or flashing QSPI.

Default is a dry-run. --root stages a test filesystem; only --execute changes it.
Never overwrites existing kit/config/service/overlay. Uninstall refuses changed
managed files or boot configuration rather than deleting user modifications.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
from configure import configure
from discover import discover

SOURCE=Path(__file__).resolve().parents[1]
PREFIX=Path('opt/voicedsp-v2-48k')
CONFIG=Path('etc/voicedsp-v2-48k')
UNIT=Path('etc/systemd/system/voicedsp-v2-48k.service')
SETUP_UNIT=Path('etc/systemd/system/voicedsp-v2-48k-setup.service')
STATE=CONFIG/'installation.json'
OVERLAY=Path('boot/firmware/overlays/voicedsp-v2-48k.dtbo')
BOOT=Path('boot/firmware/config.txt')


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def service(user,device,ram_boot):
    pre=(f'ExecStartPre=/usr/bin/python3 /{PREFIX}/scripts/boot_linux.py '
         f'--device {device} --image /{PREFIX}/firmware/xmos-i2s48.spi_boot.bin '
         '--confirm-v2-header --confirm-exclusive\n') if ram_boot else ''
    return f'''[Unit]
Description=Voice DSP+ V2 48 kHz
After=sound.target

[Service]
Type=simple
User={user}
SupplementaryGroups=audio i2c spi gpio
Environment=PYTHONDONTWRITEBYTECODE=1
RuntimeDirectory=voicedsp-v2-48k
RuntimeDirectoryMode=0700
{pre}ExecStart=/usr/bin/python3 /{PREFIX}/scripts/run.py --config-dir /{CONFIG} --device {device} --socket /run/voicedsp-v2-48k/control.sock --paired-image
Restart=no
TimeoutStopSec=10
KillMode=mixed

[Install]
WantedBy=multi-user.target
'''


def verify_source(source):
    manifest=json.loads((source/'manifest.json').read_text())
    for relative,expected in manifest['files'].items():
        path=(source/relative).resolve()
        if not path.is_relative_to(source.resolve()) or digest(path)!=expected:
            raise ValueError('Kit hash mismatch: '+relative)
    binary=source/'build/voicedsp-pi48'
    header=binary.read_bytes()[:20]
    machine=int.from_bytes(header[18:20],'little')
    expected={'aarch64':183,'armv7l':40,'x86_64':62}.get(platform.machine())
    if header[:4]!=b'\x7fELF' or header[5]!=1 or machine!=expected:
        raise ValueError('Compile the controller locally for this Linux architecture')


def install(root,source,user,device,card,playback,capture,ram_boot,overlay):
    verify_source(source)
    for path in (PREFIX,CONFIG,UNIT):
        if (root/path).exists():raise FileExistsError(root/path)
    if overlay and ((root/OVERLAY).exists() or not (root/BOOT).is_file()):
        raise ValueError('Requires unoccupied overlay name and /boot/firmware/config.txt')
    if overlay:
        text=(root/BOOT).read_text()
        if 'voicedsp-v2-48k' in text:raise ValueError('Boot config already references this kit')
        if re.search(r'^\s*dtoverlay=.*(?:i2s|audio|voice|hifi|wm8960|adau)',text,re.M|re.I):
            raise ValueError('Review conflicting audio overlays first; none removed automatically')
        subprocess.run(['dtc','-@','-I','dts','-O','dtb','-o',str(source/'build/voicedsp-v2-48k.dtbo'),
                        str(source/'host/v2/pi/voicedsp-v2-overlay.dts')],check=True)
    created=[]
    original=None
    try:
        (root/PREFIX).parent.mkdir(parents=True,exist_ok=True)
        created.append(root/PREFIX)
        shutil.copytree(source,root/PREFIX,ignore=shutil.ignore_patterns('__pycache__','.git','build'))
        (root/PREFIX/'build').mkdir()
        shutil.copy2(source/'build/voicedsp-pi48',root/PREFIX/'build/voicedsp-pi48')
        (root/PREFIX/'build/voicedsp-pi48').chmod(0o755)
        created.append(root/CONFIG)
        configure(root/CONFIG,card,playback,capture)
        (root/UNIT).parent.mkdir(parents=True,exist_ok=True)
        created.append(root/UNIT)
        (root/UNIT).write_text(service(user,device,ram_boot))
        report={'version':1,'service_enabled':False,'ram_boot':ram_boot,'files':{},'boot':None}
        if overlay:
            (root/OVERLAY).parent.mkdir(parents=True,exist_ok=True)
            created.append(root/OVERLAY)
            shutil.copy2(source/'build/voicedsp-v2-48k.dtbo',root/OVERLAY)
            original=(root/BOOT).read_bytes()
            block='\n# BEGIN VoiceDSP V2 Pi48\n[all]\ndtparam=i2c_arm=on\ndtparam=spi=on\ndtoverlay=voicedsp-v2-48k\n# END VoiceDSP V2 Pi48\n'
            (root/BOOT).write_bytes(original+block.encode())
            report['boot']={'original_b64':base64.b64encode(original).decode(),
                            'installed_sha256':digest(root/BOOT)}
        for path in created:
            files=path.rglob('*') if path.is_dir() else [path]
            for file in files:
                if file.is_file():report['files'][file.relative_to(root).as_posix()]=digest(file)
        (root/STATE).write_text(json.dumps(report,indent=2)+'\n')
    except BaseException:
        if original is not None:(root/BOOT).write_bytes(original)
        for path in reversed(created):
            if path.is_dir():shutil.rmtree(path)
            elif path.exists():path.unlink()
        raise
    return report


def uninstall(root):
    if (root/'var/lib/voicedsp-desktop/install.json').exists():
        raise ValueError('Remove desktop bridge first: sudo python3 /opt/voicedsp-desktop/desktop.py remove')
    report=json.loads((root/STATE).read_text())
    for relative,expected in report['files'].items():
        path=(root/relative).resolve()
        allowed=path.is_relative_to((root/PREFIX).resolve()) or path.is_relative_to((root/CONFIG).resolve()) or path in ((root/UNIT).resolve(),(root/SETUP_UNIT).resolve(),(root/OVERLAY).resolve())
        if not allowed or digest(path)!=expected:
            raise ValueError('Refusing removal of modified/invalid path: '+relative)
    if report['boot'] and digest(root/BOOT)!=report['boot']['installed_sha256']:
        raise ValueError('Boot config changed: reconcile manually before uninstall')
    if report['boot']:
        (root/BOOT).write_bytes(base64.b64decode(report['boot']['original_b64']))
    # Only managed files are removed. Leave any new user files/directories alone.
    for relative in report['files']:
        (root/relative).unlink()
    (root/STATE).unlink()
    for folder in (root/PREFIX,root/CONFIG):
        for sub in sorted((p for p in folder.rglob('*') if p.is_dir()),key=lambda p:len(p.parts),reverse=True):
            if not any(sub.iterdir()):sub.rmdir()
        if not any(folder.iterdir()):folder.rmdir()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=('install','uninstall'))
    p.add_argument('--root',type=Path,default=Path('/'))
    p.add_argument('--user')
    p.add_argument('--device')
    p.add_argument('--card')
    p.add_argument('--playback-device',type=int,default=0)
    p.add_argument('--capture-device',type=int,default=1)
    p.add_argument('--ram-boot',action='store_true')
    p.add_argument('--install-overlay',action='store_true')
    p.add_argument('--confirm-v2-exclusive',action='store_true')
    p.add_argument('--execute',action='store_true')
    a=p.parse_args();root=a.root.resolve()
    if a.action=='install' and (not a.user or not re.fullmatch(r'[a-z_][a-z0-9_-]*',a.user) or not a.device or not re.fullmatch(r'/dev/i2c-\d+',a.device) or not a.card):
        p.error('Provide --user, --device /dev/i2c-N and --card ALSA_ID')
    if not a.execute:
        print(json.dumps({'action':a.action,'root':str(root),'execute':False,'ram_boot':a.ram_boot,'install_overlay':a.install_overlay}));return
    if root==Path('/'):
        if os.geteuid()!=0 or not a.confirm_v2_exclusive:p.error('Requires root and --confirm-v2-exclusive')
        if subprocess.run(['systemctl','is-active','--quiet','voicedsp-v2-48k.service']).returncode==0:
            p.error('Stop the service before changing its installation')
        if a.action=='uninstall':
            if (root/SETUP_UNIT).exists():
                subprocess.run(['systemctl','disable','--now','voicedsp-v2-48k-setup.service'],check=True)
            subprocess.run(['systemctl','disable','voicedsp-v2-48k.service'],check=True)
    if a.action=='install':
        if a.card == 'auto':
            detected = discover()
            a.card = detected['card']
            a.playback_device = detected['playback_device']
            a.capture_device = detected['capture_device']
            print(json.dumps({'detected_audio': detected}))
        install(root,SOURCE,a.user,a.device,a.card,a.playback_device,a.capture_device,a.ram_boot,a.install_overlay)
    else:uninstall(root)
    if root==Path('/'):subprocess.run(['systemctl','daemon-reload'],check=True)
    print('Files updated. Service not started; no audio, SPI transfer or QSPI programming performed.')


if __name__=='__main__':main()

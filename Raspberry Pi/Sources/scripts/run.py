"""48k-only clock supervisor. Does not provision or flash the XMOS."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'host/v2/pi'))
from clock_supervisor import AlsaSilence, checked_config, parent_death_guard, supervise


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config-dir', type=Path, required=True)
    p.add_argument('--device', required=True)
    p.add_argument('--socket', required=True)
    p.add_argument('--paired-image', action='store_true', required=True)
    p.add_argument('--resume', action='store_true')
    a = p.parse_args()
    config = checked_config(a.config_dir.resolve(), 48000)
    os.environ['ALSA_CONFIG_PATH'] = str(config)
    stopped = [False]
    def stop(_sig, _frame):
        stopped[0] = True
    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    executable = ROOT / 'build/voicedsp-pi48'
    if not executable.is_file():
        raise RuntimeError('Build the kit for this Linux architecture first')
    args = [str(executable), '--device', a.device, '--socket', a.socket,
            '--paired-image', '--external-clocks']
    if a.resume:
        args.append('--resume')
    parent = os.getpid()
    def launch():
        return subprocess.Popen(args, start_new_session=True,
                                preexec_fn=lambda: parent_death_guard(parent))
    result = supervise(AlsaSilence('voicedsp_v2_dmix', 48000), launch, lambda: stopped[0])
    print(json.dumps(result), flush=True)
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())

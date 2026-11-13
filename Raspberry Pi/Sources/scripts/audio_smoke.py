"""Explicit low-level internal speaker/processed capture smoke test, not AEC qualification."""
import argparse
from array import array
import json
import math
import os
from pathlib import Path
import subprocess
import time
import wave
from control import request


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--socket', default='/run/voicedsp-v2-48k/control.sock')
    p.add_argument('--config', default='/etc/voicedsp-v2-48k/voicedsp-v2.conf')
    p.add_argument('--play-internal', action='store_true', required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    control = lambda cmd: request(a.socket, cmd)
    before = control('status')
    if not before['ready'] or before['mic_muted']:
        raise RuntimeError('Requires ready controller and unmuted microphone')
    env = dict(os.environ, ALSA_CONFIG_PATH=a.config)
    rate = 48000
    signal = array('h')
    for sec in range(6):
        hz = 700 if sec % 2 == 0 else 1100
        for n in range(rate):
            envelope = min(1.0, n / 480, (rate - 1 - n) / 480)
            signal.append(int(32767 * .2 * envelope * math.sin(2 * math.pi * hz * n / rate)))
    stimulus = a.output / 'stimulus.wav'
    with wave.open(str(stimulus), 'wb') as w:
        w.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        w.writeframes(signal.tobytes())
    capture = None
    report = {'before': before, 'stimulus_dbfs_peak': 20 * math.log10(.2),
              'speaker_level_db': -24, 'aec_modified': False}
    try:
        control('gestures off')
        control('output off')
        control('volume -24')
        control('output internal')
        control('mute off')
        report['during'] = control('status')
        with (a.output / 'capture.log').open('w') as log:
            capture = subprocess.Popen(['arecord', '-D', 'voicedsp_v2_processed',
                '-r', str(rate), '-c', '1', '-f', 'S32_LE', '-d', '12',
                str(a.output / 'processed.wav')], env=env, stdout=log, stderr=log)
            time.sleep(2)
            playback = subprocess.run(['aplay', '-D', 'voicedsp_v2_playback', str(stimulus)],
                                      env=env, capture_output=True, text=True, timeout=15)
            (a.output / 'playback.log').write_text(playback.stdout + playback.stderr)
            report['playback_exit'] = playback.returncode
            report['capture_exit'] = capture.wait(timeout=20)
    finally:
        if capture is not None and capture.poll() is None:
            capture.terminate()
            capture.wait(timeout=5)
        control('output off')
        control('volume ' + str(int(before['volume_db'])))
        # Leave all outputs off after an unattended test; no spontaneous playback.
        control('gestures on')
        report['after'] = control('status')
        (a.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    with wave.open(str(a.output / 'processed.wav'), 'rb') as w:
        data = array('i', w.readframes(w.getnframes()))
        report['capture_format'] = [w.getframerate(), w.getnchannels(), w.getsampwidth()]
    windows = []
    for start in range(0, len(data) - rate + 1, rate):
        samples = [v / 2147483648 for v in data[start:start + rate]]
        rms = math.sqrt(sum(x*x for x in samples) / rate)
        windows.append({'second': start // rate, 'rms_dbfs': 20*math.log10(max(rms, 1e-15)),
                        'peak': max(map(abs, samples)),
                        'nonzero': sum(x != 0 for x in samples)})
    report['capture_windows'] = windows
    (a.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    if report['capture_exit'] or report['playback_exit'] or not any(data):
        raise RuntimeError('PCM smoke test failed')


if __name__ == '__main__':
    main()

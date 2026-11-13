"""Capture raw microphone and far-end reference together through the sole owner."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from control import request


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--beat', type=Path, required=True)
    p.add_argument('--route', choices=('internal', 'external', 'full'), default='internal')
    p.add_argument('--gain', type=int, default=-12)
    args = p.parse_args()
    assert -30 <= args.gain <= -6
    args.output.mkdir(parents=True, exist_ok=False)
    ctl = lambda cmd: request('/run/voicedsp-v2-48k/control.sock', cmd)
    before = ctl('status')
    result = {'before': before, 'timeline': [], 'route': args.route, 'gain': args.gain}
    env = dict(os.environ, ALSA_CONFIG_PATH='/etc/voicedsp-v2-48k/voicedsp-v2.conf')
    recording = player = None
    try:
        assert before['ready'] and not before['jack']
        ctl('gestures off'); ctl('output off'); ctl('volume '+str(args.gain))
        ctl('output '+args.route); ctl('mute off'); ctl('capture alignment')
        deadline = time.monotonic()+15
        while ctl('status')['aec_mux'] != [3, 0, 12, 0]:
            if time.monotonic() > deadline: raise RuntimeError('Alignment mux timeout')
            time.sleep(.2)
        with (args.output/'capture.log').open('w') as capture, (args.output/'playback.log').open('w') as playlog:
            recording = subprocess.Popen(['arecord','-D','voicedsp_v2_capture','-r48000','-c2','-fS32_LE',
                '--buffer-size=8192','--period-size=1024','-d18',str(args.output/'mic-reference.wav')],
                env=env,stdout=capture,stderr=capture)
            time.sleep(1)
            player = subprocess.Popen(['aplay','-D','voicedsp_v2_playback',str(args.beat)],env=env,stdout=playlog,stderr=playlog)
            deadline = time.monotonic()+22
            while recording.poll() is None:
                state = ctl('status'); result['timeline'].append(state)
                assert state['ready'] and state['aec_mux']==[3,0,12,0] and not state['aec_error']
                if time.monotonic()>deadline: raise RuntimeError('Capture timeout')
                time.sleep(.5)
            assert recording.returncode==0
        result['complete']=True
    finally:
        for proc in (recording,player):
            if proc and proc.poll() is None: proc.terminate();proc.wait(timeout=5)
        ctl('output off');ctl('capture processed');time.sleep(.5)
        ctl('volume '+str(int(before['volume_db'])))
        ctl('gestures '+('on' if before['gestures_enabled'] else 'off'))
        result['restored']=ctl('status')
        (args.output/'report.json').write_text(json.dumps(result,indent=2)+'\n')
    for name in ('capture','playback'):
        log=(args.output/(name+'.log')).read_text().lower()
        assert 'overrun' not in log and 'underrun' not in log
    print(json.dumps({'complete':True,'output':str(args.output)}))


if __name__=='__main__': main()

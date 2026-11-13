"""One bounded AEC capture through the sole controller, restoring processed/OFF."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import shutil
import tempfile
import time
from control import request


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--beat', type=Path, required=True)
    p.add_argument('--kind', choices=('voice','echo','doubletalk'), required=True)
    p.add_argument('--raw', action='store_true')
    p.add_argument('--gain', type=int, default=-12)
    p.add_argument('--route', choices=('internal','external','full'), default='internal')
    p.add_argument('--profile', choices=('default','voice'), default='default')
    a = p.parse_args()
    if not -30 <= a.gain <= -6:
        raise ValueError('Bench gain must be -30..-6 dB')
    a.output.mkdir(parents=True, exist_ok=False)
    ctl = lambda cmd: request('/run/voicedsp-v2-48k/control.sock', cmd)
    before = ctl('status')
    env = dict(os.environ, ALSA_CONFIG_PATH='/etc/voicedsp-v2-48k/voicedsp-v2.conf')
    report = {'before': before, 'kind': a.kind, 'raw': a.raw, 'gain': a.gain,
              'route': a.route, 'profile': a.profile, 'timeline': []}
    recorder = player = None
    handles = []
    scratch = tempfile.TemporaryDirectory(prefix='voicedsp-aec-', dir='/dev/shm')
    capture_path = Path(scratch.name)/'VoiceDSP.wav'
    try:
        assert before['ready'] and not before['mic_muted'] and not before['jack']
        ctl('gestures off'); ctl('output off'); ctl('volume '+str(a.gain))
        ctl('aec-profile '+a.profile)
        ctl('output '+a.route); ctl('mute off')
        ctl('capture raw' if a.raw else 'capture processed')
        expected = [3,0,3,1] if a.raw else [8,0,8,1]
        deadline = time.monotonic()+15
        while True:
            state = ctl('status')
            if state['aec_mux_valid'] and state['aec_mux'] == expected and state['aec_parameter_mask']==0xffffff:
                break
            if time.monotonic()>deadline:
                raise RuntimeError('Mux/parameter readback timeout')
            time.sleep(.2)
        report['parameters_before'] = state
        if a.route == 'full':
            assert state['full_speaker'] and state['crossover_verified']
        for name in ('capture','playback'):
            handles.append((a.output/(name+'.log')).open('w'))
        recorder = subprocess.Popen(['arecord','-D','voicedsp_v2_processed','-r48000','-c1','-fS32_LE',
            '--buffer-size=8192','--period-size=1024','-d38',str(capture_path)],env=env,stdout=handles[0],stderr=handles[0])
        report['capture_start_ns'] = time.monotonic_ns()
        time.sleep(.25)
        if a.kind != 'voice':
            player = subprocess.Popen(['aplay','-D','voicedsp_v2_playback',str(a.beat)],
                                      env=env,stdout=handles[1],stderr=handles[1])
        report['playback_start_ns'] = time.monotonic_ns()
        print('READY '+json.dumps({'kind':a.kind,'raw':a.raw}),flush=True)
        while recorder.poll() is None:
            state=ctl('status')
            report['timeline'].append({'ns':time.monotonic_ns(),'state':state})
            assert state['ready'] and state['actual_route']==state['requested_route'] and state['actual_route']!=1
            assert state['volume_db']==a.gain
            if a.route == 'full':
                assert state['full_speaker'] and state['crossover_verified']
            assert not state['speaker_muted'] and not state['mic_muted'] and state['aec_valid']
            assert state['aec_mux_valid'] and state['aec_mux']==expected and not state['aec_error']
            if time.monotonic_ns()-report['capture_start_ns']>45_000_000_000:
                raise RuntimeError('Capture timeout')
            time.sleep(.5)
        report['capture_exit']=recorder.returncode
        report['playback_exit']=player.wait(timeout=10) if player else 0
        report['parameters_after']=ctl('status')
        assert report['capture_exit']==report['playback_exit']==0
        for handle in handles:handle.flush()
        for name in ('capture','playback'):
            log=(a.output/(name+'.log')).read_text().lower()
            if 'overrun' in log or 'underrun' in log:
                raise RuntimeError(name+' XRUN: take invalid even if ALSA recovered')
        report['complete']=True
    finally:
        for proc in (player,recorder):
            if proc and proc.poll() is None:
                proc.terminate();proc.wait(timeout=5)
        for f in handles:f.close()
        # Preserve even rejected takes, but do not write megabytes to the SD
        # card while playback/capture are running.
        if capture_path.exists():shutil.copy2(capture_path,a.output/'VoiceDSP.wav')
        scratch.cleanup()
        ctl('output off');ctl('capture processed')
        ctl('aec-profile '+('legacy' if before.get('aec_legacy_override') else
                           'voice' if before.get('aec_voice_override') else 'default'))
        time.sleep(.5)
        ctl('volume '+str(int(before['volume_db'])))
        ctl('gestures '+('on' if before['gestures_enabled'] else 'off'))
        report['restored']=ctl('status')
        (a.output/'take.json').write_text(json.dumps(report,indent=2)+'\n')
    print('DONE',flush=True)


if __name__ == '__main__':main()

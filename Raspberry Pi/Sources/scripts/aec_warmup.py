"""Bounded far-end adaptation, fixed gain, no source/volume changes during a run."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from control import request


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--beat',type=Path,required=True)
    p.add_argument('--route',choices=('internal','full'),required=True)
    p.add_argument('--profile',choices=('default','voice'),default='voice')
    p.add_argument('--gain',type=int,default=-12)
    p.add_argument('--seconds',type=int,default=70)
    a=p.parse_args()
    assert -30<=a.gain<=-6 and 10<=a.seconds<=120
    a.output.mkdir(parents=True,exist_ok=False)
    ctl=lambda cmd:request('/run/voicedsp-v2-48k/control.sock',cmd)
    before=ctl('status');result={'before':before,'route':a.route,'gain':a.gain,'states':[]}
    player=None
    env=dict(os.environ,ALSA_CONFIG_PATH='/etc/voicedsp-v2-48k/voicedsp-v2.conf')
    try:
        assert before['ready'] and not before['jack']
        ctl('gestures off');ctl('output off');ctl('volume '+str(a.gain));ctl('aec-profile '+a.profile)
        ctl('output '+a.route);ctl('mute off');ctl('capture processed')
        with (a.output/'playback.log').open('w') as log:
            start=time.monotonic()
            while time.monotonic()-start<a.seconds:
                state=ctl('status')
                assert state['ready'] and state['volume_db']==a.gain and not state['speaker_muted']
                if a.route=='full':assert state['full_speaker'] and state['crossover_verified']
                if player is None or player.poll() is not None:
                    if player is not None:assert player.returncode==0
                    player=subprocess.Popen(['aplay','-D','voicedsp_v2_playback',str(a.beat)],
                                            env=env,stdout=log,stderr=log)
                result['states'].append({'s':time.monotonic()-start,'state':state})
                time.sleep(.5)
        result['complete']=True
    finally:
        if player and player.poll() is None:player.terminate();player.wait(timeout=5)
        ctl('output off');ctl('capture processed')
        ctl('aec-profile '+('legacy' if before.get('aec_legacy_override') else
                           'voice' if before.get('aec_voice_override') else 'default'))
        ctl('volume '+str(int(before['volume_db'])))
        ctl('gestures '+('on' if before['gestures_enabled'] else 'off'))
        result['restored']=ctl('status')
        (a.output/'report.json').write_text(json.dumps(result,indent=2)+'\n')
    assert 'underrun' not in (a.output/'playback.log').read_text().lower()
    print(json.dumps({'complete':True,'final_converged':result['states'][-1]['state']['aec_parameter_words'][2]}))


if __name__=='__main__':main()

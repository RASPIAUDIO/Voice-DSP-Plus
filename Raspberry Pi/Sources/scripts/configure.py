"""Generate a 48k mono ALSA config and overlay without installing anything."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from prepare_v2_pi_audio import prepare


def configure(out, card, playback, capture):
    manifest = prepare(out, card, playback, capture, 48000)
    config = out / 'voicedsp-v2.conf'
    text = config.read_text().replace(
        'pcm.voicedsp_v2_playback { type plug slave.pcm "voicedsp_v2_dmix" }',
        '''# Stereo source -> (L+R)/2 on BOTH DAC slots. A mono source is duplicated
# by the outer plug to two channels before this matrix; never mix DSP capture.
pcm.voicedsp_v2_mono {
    type route
    slave { pcm "voicedsp_v2_dmix" channels 2 }
    ttable { 0.0 0.5 1.0 0.5 0.1 0.5 1.1 0.5 }
}
pcm.voicedsp_v2_playback {
    type plug
    slave { pcm "voicedsp_v2_mono" channels 2 }
    route_policy duplicate
}''')
    # Pi4 desktop scheduling can exceed the original 21 ms capture buffer.
    # This changes capture tolerance only, not playback/reference latency.
    prefix, capture = text.split('pcm.voicedsp_v2_dsnoop {', 1)
    capture = capture.replace('period_size 256', 'period_size 1024', 1)
    capture = capture.replace('buffer_size 1024', 'buffer_size 8192', 1)
    text = prefix + 'pcm.voicedsp_v2_dsnoop {' + capture
    config.write_text(text)
    manifest['outputs'][config.name] = hashlib.sha256(config.read_bytes()).hexdigest()
    manifest['mono_playback'] = 'average L/R, duplicate to both DAC slots'
    manifest['processed_capture'] = 'channel 0 only'
    (out / 'MANIFEST.json').write_text(json.dumps(manifest, indent=2)+'\n')
    return manifest


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--card', required=True)
    p.add_argument('--playback-device', type=int, required=True)
    p.add_argument('--capture-device', type=int, required=True)
    a = p.parse_args()
    print(json.dumps(configure(a.output, a.card, a.playback_device, a.capture_device), indent=2))

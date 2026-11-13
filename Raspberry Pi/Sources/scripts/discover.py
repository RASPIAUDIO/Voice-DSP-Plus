"""Read actual ALSA IDs after overlay reboot; never infer PCM link ordering."""
import argparse
import json
from pathlib import Path
import re


def discover(root=Path('/proc/asound')):
    cards = (root / 'cards').read_text()
    indices = re.findall(r'^\s*(\d+)\s+\[[^]]+\].* - VoiceDSP V2 I2S\s*$', cards, re.M)
    if len(indices) != 1:
        raise ValueError('Expected exactly one VoiceDSP V2 I2S card after overlay reboot')
    card = root / ('card' + indices[0])
    result = {'card': (card / 'id').read_text().strip()}
    for suffix, direction in (('p', 'playback'), ('c', 'capture')):
        devices = [int(p.name[3:-1]) for p in card.glob('pcm*' + suffix)
                   if re.fullmatch(r'pcm\d+' + suffix, p.name)]
        if len(devices) != 1:
            raise ValueError('Expected exactly one ' + direction + ' PCM')
        result[direction + '_device'] = devices[0]
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--proc-root', type=Path, default=Path('/proc/asound'))
    print(json.dumps(discover(parser.parse_args().proc_root)))

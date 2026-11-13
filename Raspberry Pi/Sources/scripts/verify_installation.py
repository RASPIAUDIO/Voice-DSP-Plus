"""Read-only check of the accepted Pi AEC configuration; never plays audio."""
import argparse
import json
from pathlib import Path
import time
import sys
sys.dont_write_bytecode = True
from control import request

ROOT = Path(__file__).resolve().parents[1]


def differences(state, policy, startup=False):
    expected = {
        'ready': True, 'aec_valid': True, 'aec_error': 0, 'error': 0,
        'aec_profile': policy['default_profile'], 'aec_legacy_override': False,
        'aec_mux_valid': True, 'aec_mux': policy['processed_mux'],
        'aec_parameter_mask': 0xffffff, 'aec_metadata_valid': 3,
        'aec_asr_output': policy['asr_output'], 'aec_system_delay': policy['system_delay'],
        'mic_mute_valid': True, 'mic_muted': False,
    }
    if startup:
        expected.update(requested_route=policy['startup']['route'],
                        actual_route=policy['startup']['route'],
                        volume_db=policy['startup']['volume_db'],
                        gestures_enabled=policy['startup']['gestures_enabled'],
                        amp_sd_n=False, speaker_muted=False)
        expected.update(crossover_hz=policy['full_speaker']['cutoff_hz'],
                        woofer_trim_db=policy['full_speaker']['woofer_trim_db'],
                        tweeter_trim_db=policy['full_speaker']['tweeter_trim_db'])
    errors = [key for key, value in expected.items() if state.get(key) != value]
    words = state.get('aec_parameter_words', [])
    if len(words) != len(policy['expected_parameter_words']):
        errors.append('aec_parameter_words length')
    else:
        for name, actual, value in zip(policy['parameter_names'], words, policy['expected_parameter_words']):
            if value is not None and actual != value:
                errors.append(name)
    return errors


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--socket', default='/run/voicedsp-v2-48k/control.sock')
    p.add_argument('--timeout', type=float, default=30)
    p.add_argument('--startup', action='store_true', help='Also require OFF/-30 dB startup defaults')
    a = p.parse_args()
    if not 0 <= a.timeout <= 120:
        p.error('timeout must be 0..120 seconds')
    policy = json.loads((ROOT / 'accepted-aec.json').read_text())
    deadline = time.monotonic() + a.timeout
    while True:
        state = None
        try:
            state = request(a.socket, 'status')
            errors = differences(state, policy, a.startup)
        except (OSError, RuntimeError, ValueError) as exc:
            errors = [str(exc)]
        if not errors or time.monotonic() >= deadline:
            print(json.dumps({'passed': not errors, 'kit_version': policy['kit_version'],
                              'checkpoint': policy['checkpoint_tag'], 'mismatches': errors,
                              'state': state, 'acoustic_test': False}, indent=2))
            return 1 if errors else 0
        time.sleep(.5)


if __name__ == '__main__':
    raise SystemExit(main())

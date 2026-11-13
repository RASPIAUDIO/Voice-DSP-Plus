import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from verify_installation import differences


class AcceptedPolicyTests(unittest.TestCase):
    def setUp(self):
        self.policy = json.loads((ROOT / 'accepted-aec.json').read_text())
        self.state = dict(ready=True, aec_valid=True, aec_error=0, error=0,
                          aec_profile=4, aec_legacy_override=False,
                          aec_mux_valid=True, aec_mux=[8, 0, 8, 1],
                          aec_parameter_mask=0xffffff, aec_metadata_valid=3,
                          aec_asr_output=0, aec_system_delay=-32,
                          mic_mute_valid=True, mic_muted=False,
                          actual_route=1, requested_route=1, volume_db=-30,
                          gestures_enabled=True, amp_sd_n=False, speaker_muted=False,
                          crossover_hz=9500, woofer_trim_db=0, tweeter_trim_db=-6,
                          aec_parameter_words=[x if x is not None else 0
                                               for x in self.policy['expected_parameter_words']])

    def test_accepted_startup(self):
        self.assertEqual(differences(self.state, self.policy, True), [])

    def test_dynamic_convergence_not_required(self):
        self.state['aec_parameter_words'][2:4] = [1, 1]
        self.assertEqual(differences(self.state, self.policy, True), [])

    def test_every_fixed_word_checked(self):
        for i, word in enumerate(self.policy['expected_parameter_words']):
            if word is not None:
                state = copy.deepcopy(self.state)
                state['aec_parameter_words'][i] ^= 1
                self.assertIn(self.policy['parameter_names'][i], differences(state, self.policy))

    def test_rejects_legacy_and_wrong_mux(self):
        self.state.update(aec_profile=6, aec_legacy_override=True, aec_mux=[3, 0, 3, 1])
        self.assertEqual(set(differences(self.state, self.policy)),
                         {'aec_profile', 'aec_legacy_override', 'aec_mux'})

    def test_startup_checks_are_explicit(self):
        self.state.update(actual_route=4, requested_route=4, volume_db=-6)
        self.assertEqual(differences(self.state, self.policy), [])
        self.assertEqual(set(differences(self.state, self.policy, True)),
                         {'actual_route', 'requested_route', 'volume_db'})

    def test_missing_state_fails(self):
        self.assertTrue(differences({}, self.policy, True))


if __name__ == '__main__':
    unittest.main()

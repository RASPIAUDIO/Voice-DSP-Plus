import sys
from pathlib import Path
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import desktop


class DesktopTests(unittest.TestCase):
    def test_fragment_does_not_recurse(self):
        result = desktop.fragment('</usr/share/alsa/alsa.conf>\npcm.test { type null }\n')
        self.assertNotIn('</usr/share/alsa/alsa.conf>', result)
        self.assertIn('pcm.test { type null }', result)

    def test_unknown_base_refused(self):
        for text in ('pcm.test {}', '</usr/share/alsa/alsa.conf>\n' * 2):
            with self.assertRaises(ValueError):
                desktop.fragment(text)

    def test_native_profile_uses_accepted_pc_ms(self):
        text = (desktop.DATA / 'voicedsp-v2.conf').read_text()
        self.assertIn('device-strings = voicedsp_v2_playback', text)
        self.assertIn('device-strings = plug:voicedsp_v2_processed', text)
        self.assertIn('channel-map = mono', text)
        self.assertIn('channel-map = front-left,front-right', text)
        self.assertIn('paths-input = analog-input', text)
        self.assertIn('paths-output = analog-output', text)
        self.assertIn('skip-probe = no', text)
        self.assertNotIn('hw:', text)

    def test_native_rules_preserve_clock_keeper(self):
        text = (desktop.DATA / '90-voicedsp-v2.conf').read_text()
        self.assertIn('alsa.udev.expose-busy = true', text)
        self.assertIn('device.profile = "off"', text)
        self.assertEqual(text.count('audio.rate = 48000'), 2)
        self.assertNotIn('device.disabled', text)

    def test_missing_card_is_retried_not_replaced_by_cardless_nodes(self):
        with patch.object(desktop, 'run', return_value='[]'):
            with self.assertRaises(RuntimeError):
                desktop.activate_card()

    def test_native_card_idempotent_and_requires_ports(self):
        import json
        card = {'name': desktop.CARD, 'profiles': {'voicedsp-duplex': {}},
                'active_profile': 'voicedsp-duplex'}
        nodes = [[{'name': name, 'ports': [{}], 'properties': {'alsa.card': '2'}}]
                 for name in (desktop.SINK, desktop.SOURCE)]
        with patch.object(desktop, 'run', side_effect=[json.dumps([card]), *map(json.dumps, nodes)]) as run:
            desktop.activate_card()
        self.assertEqual(run.call_count, 3)
        nodes[0][0]['ports'] = []
        with patch.object(desktop, 'run', side_effect=[json.dumps([card]), json.dumps(nodes[0])]):
            with self.assertRaises(RuntimeError):
                desktop.activate_card()

    def test_user_service_waits_for_pulse_and_retries(self):
        text = desktop.unit()
        self.assertIn('After=pipewire-pulse.service wireplumber.service', text)
        self.assertIn('Restart=on-failure', text)
        self.assertIn('PartOf=pipewire-pulse.service', text)
        self.assertNotIn('sudo', text)

    def test_core_gate_needs_no_pulse_operations(self):
        root = Path(__file__).resolve().parents[1]
        with patch.object(desktop, 'PREFIX', root), \
             patch('control.request', return_value={'ready': True}), \
             patch('verify_installation.differences', return_value=[]), \
             patch.object(desktop, 'run') as run:
            self.assertEqual(desktop.wait_core(), {'ready': True})
        run.assert_not_called()

    def test_unavailable_core_does_not_block_other_cards_forever(self):
        root = Path(__file__).resolve().parents[1]
        with patch.object(desktop, 'PREFIX', root), \
             patch('control.request', side_effect=FileNotFoundError):
            self.assertIsNone(desktop.wait_core(strict=False, timeout=0))
            with self.assertRaises(FileNotFoundError):
                desktop.wait_core(strict=True, timeout=0)


if __name__ == '__main__':
    unittest.main()

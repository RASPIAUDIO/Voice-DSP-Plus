import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import output_selector as ui


class SelectorTests(unittest.TestCase):
    def test_modes(self):
        self.assertEqual(ui.mode_of({'requested_route': 4}), 'internal')
        self.assertEqual(ui.mode_of({'requested_route': 3, 'full_speaker': True}), 'full')
        self.assertEqual(ui.mode_of({'requested_route': 3, 'full_speaker': False}), 'external')
        self.assertIsNone(ui.mode_of({'requested_route': 1}))

    def test_external_requires_confirmation_before_io(self):
        with patch.object(ui, 'request') as request, patch.object(ui, 'run') as run:
            with self.assertRaises(ValueError):
                ui.switch('external', power_confirmed=True)
        request.assert_not_called()
        run.assert_not_called()

    def test_external_modes_require_pd_before_io(self):
        for mode in ('external', 'full'):
            with self.subTest(mode=mode), patch.object(ui, 'request') as request, patch.object(ui, 'run') as run:
                with self.assertRaisesRegex(ValueError, '--confirm-pd'):
                    ui.switch(mode, confirmed=True)
                request.assert_not_called()
                run.assert_not_called()

    def test_warning_scope_and_wiring(self):
        self.assertIsNone(ui.confirmation_text('internal'))
        for mode in ('external', 'full'):
            text = ui.confirmation_text(mode)
            self.assertIn('65 W', text)
            self.assertIn('40-pin header', text)
            self.assertIn('disconnect the Pi', text)
            self.assertIn('Never power both USB-C inputs', text)
        self.assertIn('unfiltered', ui.confirmation_text('external'))
        self.assertNotIn('unfiltered', ui.confirmation_text('full'))

    @unittest.skipUnless(sys.platform == 'linux', 'Linux lock')
    def test_route_preserves_aec_volume_and_mute(self):
        before = dict(ready=True, aec_valid=True, requested_route=4, aec_parameter_words=[1, 2])
        after = dict(before, requested_route=3, full_speaker=True, crossover_verified=True)
        for muted in (False, True):
            with tempfile.TemporaryDirectory() as folder, patch.dict(os.environ, XDG_RUNTIME_DIR=folder), \
                 patch.object(ui, 'request', side_effect=[before, {}, after]) as request, \
                 patch.object(ui, 'run', side_effect=[json.dumps([{'name': ui.SINK, 'mute': muted}]), '', '']) as run:
                self.assertEqual(ui.switch('full', power_confirmed=True), after)
                self.assertEqual([c.args[1] for c in request.call_args_list], ['status', 'output full', 'status'])
                self.assertEqual(run.call_args_list[-1].args, ('pactl', 'set-sink-mute', ui.SINK, str(int(muted))))
                self.assertFalse(any('volume' in arg for c in run.call_args_list for arg in c.args))

    @unittest.skipUnless(sys.platform == 'linux', 'Linux lock')
    def test_changed_aec_leaves_output_muted(self):
        before = dict(ready=True, aec_valid=True, requested_route=4, aec_parameter_words=[1])
        after = dict(before, aec_parameter_words=[2])
        with tempfile.TemporaryDirectory() as folder, patch.dict(os.environ, XDG_RUNTIME_DIR=folder), \
             patch.object(ui, 'request', side_effect=[before, {}, after]), \
             patch.object(ui, 'run', side_effect=[json.dumps([{'name': ui.SINK, 'mute': False}]), '']) as run:
            with self.assertRaises(RuntimeError):
                ui.switch('internal')
            self.assertEqual(run.call_count, 2)
            self.assertEqual(run.call_args_list[-1].args[-1], '1')

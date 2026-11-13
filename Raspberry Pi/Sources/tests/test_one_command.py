import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import install
import one_command as setup


@unittest.skipUnless(sys.platform == 'linux', 'Linux installation fixtures, no hardware access')
class OneCommandTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        boot = self.root / install.BOOT
        boot.parent.mkdir(parents=True)
        boot.write_text('# Original Pi config\n[all]\n')
        self.calls = []
        self.audio = {'card': 'I2S', 'playback_device': 1, 'capture_device': 0}

    def stage(self):
        install.install(self.root, ROOT, 'testuser', '/dev/i2c-1', 'I2S', 0, 1, True, True)
        setup.stage_setup(self.root, 'testuser', 'before')

    def finish(self, **kwargs):
        return setup.finish(self.root, runner=self.calls.append,
                            get_audio=lambda: self.audio, boot_id='after', **kwargs)

    def test_fresh_then_occupied_refusal(self):
        setup.require_fresh(self.root)
        self.stage()
        with self.assertRaises(FileExistsError):
            setup.require_fresh(self.root)

    def test_overlay_conflict_preserved(self):
        (self.root / install.BOOT).write_text('dtoverlay=wm8960-soundcard\n')
        with self.assertRaises(ValueError):
            setup.require_fresh(self.root)
        self.assertEqual((self.root / install.BOOT).read_text(), 'dtoverlay=wm8960-soundcard\n')

    def test_verified_after_reboot_and_uninstall(self):
        self.stage()
        self.finish()
        state = setup.verify_managed(self.root)
        self.assertEqual(state['one_command']['status'], 'verified')
        self.assertTrue(state['service_enabled'])
        self.assertFalse((self.root / setup.PENDING).exists())
        self.assertEqual(self.calls[0], ['systemctl', 'start', setup.NAME])
        self.assertIn('verify_installation.py', str(self.calls[1]))
        self.assertEqual(self.calls[2], ['systemctl', 'enable', setup.NAME])
        text = (self.root / install.CONFIG / 'voicedsp-v2.conf').read_text()
        self.assertIn('hw:I2S,1', text)
        self.assertIn('hw:I2S,0', text)
        count = len(self.calls)
        self.finish()
        self.assertEqual(len(self.calls), count)
        install.uninstall(self.root)
        self.assertFalse((self.root / install.PREFIX).exists())
        self.assertFalse((self.root / setup.SETUP).exists())

    def test_pending_uninstall(self):
        self.stage()
        install.uninstall(self.root)
        self.assertFalse((self.root / install.CONFIG).exists())
        self.assertFalse((self.root / setup.SETUP).exists())

    def test_desktop_only_after_aec_check(self):
        self.stage()
        setup.stage_setup(self.root, 'testuser', 'before', desktop=True)
        self.finish()
        desktop_calls = [c for c in self.calls if 'scripts/desktop.py' in str(c)]
        self.assertEqual(len(desktop_calls), 1)
        self.assertGreater(self.calls.index(desktop_calls[0]), 1)
        self.assertIn('testuser', desktop_calls[0])

    def test_no_same_boot_start(self):
        self.stage()
        with self.assertRaises(ValueError):
            setup.finish(self.root, runner=self.calls.append, boot_id='before')
        self.assertEqual(self.calls, [])

    def test_changed_config_refused(self):
        self.stage()
        file = self.root / install.CONFIG / 'voicedsp-v2.conf'
        file.write_text('user configuration')
        with self.assertRaises(ValueError):
            self.finish()
        self.assertEqual(file.read_text(), 'user configuration')
        self.assertEqual(self.calls, [])

    def test_invalid_discovery_never_starts_audio(self):
        self.stage()
        def missing():
            raise ValueError('No unique VoiceDSP card')
        with self.assertRaises(ValueError):
            setup.finish(self.root, runner=self.calls.append, get_audio=missing,
                         boot_id='after', sleep=lambda _: None)
        self.assertEqual(self.calls, [['systemctl', 'disable', '--now', setup.NAME]])
        self.assertTrue((self.root / setup.PENDING).exists())

    def test_failed_aec_never_enables_service(self):
        self.stage()
        def runner(args):
            self.calls.append(args)
            if args[0] == 'runuser':
                raise subprocess.CalledProcessError(1, args)
        with self.assertRaises(subprocess.CalledProcessError):
            setup.finish(self.root, runner=runner, get_audio=lambda: self.audio, boot_id='after')
        self.assertEqual(self.calls[-1], ['systemctl', 'disable', '--now', setup.NAME])
        self.assertNotIn(['systemctl', 'enable', setup.NAME], self.calls)
        self.assertTrue((self.root / setup.PENDING).exists())
        setup.verify_managed(self.root)

    def test_generate_failure_keeps_config(self):
        self.stage()
        before = (self.root / install.STATE).read_bytes()
        with patch.object(setup, 'configure', side_effect=ValueError('injected')):
            with self.assertRaises(ValueError):
                setup.configure_detected(self.root, self.audio)
        self.assertEqual(before, (self.root / install.STATE).read_bytes())
        setup.verify_managed(self.root)

    def test_payload_hash_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / 'manifest.json').write_text(json.dumps({'files': {'x': 'bad'}}))
            (path / 'x').write_text('x')
            with self.assertRaises(ValueError):
                setup.verify_payload(path)


if __name__ == '__main__':
    unittest.main()

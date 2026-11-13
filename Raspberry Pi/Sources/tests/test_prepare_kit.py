import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_kit


class PreparationTests(unittest.TestCase):
    def test_source_only_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / 'kit'
            manifest = prepare_kit.prepare(out)
            self.assertFalse(manifest['hardware_installable'])
            self.assertFalse((out/'firmware').exists())
            self.assertIn('licenses/PAJ-LICENSE.txt', manifest['files'])
            self.assertIn('LICENSE', manifest['files'])
            for name, digest in manifest['files'].items():
                self.assertEqual(prepare_kit.digest(out/name), digest)

    def test_missing_firmware_before_creation(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / 'kit'
            with self.assertRaisesRegex(ValueError, 'firmware'):
                prepare_kit.prepare(out, Path(tmp))
            self.assertFalse(out.exists())

    def test_existing_output_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            marker = out/'mine.txt'
            marker.write_text('keep')
            with self.assertRaises(ValueError):
                prepare_kit.prepare(out)
            self.assertEqual(marker.read_text(), 'keep')

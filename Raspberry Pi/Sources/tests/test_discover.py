from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from discover import discover


class DiscoveryTests(unittest.TestCase):
    def test_pi4_reversed_link_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'cards').write_text(' 2 [I2S            ]: simple-card - VoiceDSP V2 I2S\n')
            card = root / 'card2'
            (card / 'pcm0c').mkdir(parents=True)
            (card / 'pcm1p').mkdir()
            (card / 'id').write_text('I2S\n')
            self.assertEqual(discover(root), {'card': 'I2S', 'playback_device': 1, 'capture_device': 0})

    def test_missing_or_ambiguous_card_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for value in ('', ' 2 [I2S]: simple-card - VoiceDSP V2 I2S\n 3 [I2S_1]: simple-card - VoiceDSP V2 I2S\n'):
                (root / 'cards').write_text(value)
                with self.assertRaises(ValueError):
                    discover(root)

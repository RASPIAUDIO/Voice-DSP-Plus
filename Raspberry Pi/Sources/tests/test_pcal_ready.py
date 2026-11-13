import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))


@unittest.skipUnless(sys.platform == 'linux', 'Linux I2C helper')
class ReadinessTests(unittest.TestCase):
    def test_ready_requires_three_reads(self):
        from boot_linux import wait_pcal
        registers = []
        self.assertEqual(wait_pcal(lambda reg: registers.append(reg), sleep=lambda _: None), 3)
        self.assertEqual(registers, [6, 2] * 3)

    def test_initial_nack_recovers_without_writes(self):
        from boot_linux import wait_pcal
        calls = []
        def read(reg):
            calls.append(reg)
            if len(calls) < 3:
                raise OSError(5, 'not ready')
            return 255
        self.assertEqual(wait_pcal(read, sleep=lambda _: None), 5)
        self.assertEqual(calls, [6, 6] + [6, 2] * 3)

    def test_timeout_is_bounded(self):
        from boot_linux import wait_pcal
        clock = [0.0]
        def sleep(seconds):
            clock[0] += seconds
        def read(_):
            raise OSError(5, 'not ready')
        with self.assertRaises(TimeoutError):
            wait_pcal(read, timeout=1, sleep=sleep, now=lambda: clock[0])
        self.assertEqual(clock[0], 1)

    def test_other_failures_not_hidden(self):
        from boot_linux import wait_pcal
        def read(_):
            raise ValueError('wrong interface')
        with self.assertRaises(ValueError):
            wait_pcal(read)

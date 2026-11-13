"""Tests use a fake USB transport only; no USB library or hardware is loaded."""
import unittest
import struct
from unittest.mock import patch
from usb_mono_control import command, decode, exchange, filter_command, filter_exchange


class FakeUSB:
    def __init__(self, ack=b"\x00"):
        self.calls = []
        self.ack = ack

    def ctrl_transfer(self, kind, req, value, index, payload, timeout):
        self.calls.append((kind, req, value, index, payload))
        if kind == 0x40:
            return len(payload)
        if value == 17:
            return self.ack
        data = bytearray(56)
        data[0], data[44], data[5] = 2, 60, 0xC1
        return bytearray(b'\0') + data


class ControlTests(unittest.TestCase):
    def test_filter_parameters(self):
        for hz, left, right in ((6000, -24, 0), (8000, -2, -8), (10000, 0, -24)):
            self.assertEqual(struct.unpack('<fff', filter_command(hz, left, right)), (hz, left, right))
        for values in ((5999,0,0), (10001,0,0), (8000,1,0), (8000,0,-25), (float('nan'),0,0), (8000,0,float('inf'))):
            with self.assertRaises(ValueError): filter_command(*values)

    def test_filter_write_requires_off_and_readback(self):
        dev = FakeFilterUSB()
        payload = filter_command(8000, -2, -8)
        result = filter_exchange(dev, payload)
        self.assertEqual(result['cutoff_hz'], 8000)
        self.assertEqual(result['tweeter_trim_db'], -8)
        self.assertEqual(dev.calls[0][4], command('off', -30, True))
        self.assertEqual([c[2] for c in dev.calls], [17,17,146,19,19,148])

    def test_filter_failures(self):
        payload = filter_command(8000, -2, -8)
        dev = FakeFilterUSB(); dev.bcdDevice = 0x81
        with self.assertRaises(ValueError): filter_exchange(dev, payload)
        self.assertEqual(dev.calls, [])
        for fault in ('reject', 'short', 'mismatch'):
            with self.subTest(fault=fault), self.assertRaises(RuntimeError):
                filter_exchange(FakeFilterUSB(fault), payload)
        dev = FakeFilterUSB('active')
        with patch('usb_mono_control.time.monotonic', side_effect=[0,4]), self.assertRaises(RuntimeError):
            filter_exchange(dev, payload)
        self.assertNotIn(19, [c[2] for c in dev.calls])

    def test_routes(self):
        self.assertEqual(command("internal", -30, False), bytes((2,4,60,0)))
        self.assertEqual(command("full-speaker", -24, True), bytes((2,3,48,3)))
        self.assertEqual(command("external", 0, False), bytes((2,3,0,0)))

    def test_invalid_levels(self):
        for db in (-60.5, 0.5, -0.1, float("nan")):
            with self.assertRaises(ValueError):
                command("internal", db, False)

    def test_wire_protocol(self):
        dev = FakeUSB()
        state = exchange(dev, command("internal", -30, False))
        self.assertEqual([c[:4] for c in dev.calls],
                         [(0x40,0,17,0x30), (0xC0,0,17,0x30), (0xC0,0,146,0x30)])
        self.assertEqual(state["level_db"], -30)
        self.assertTrue(state["aec_profiles_applied"])
        self.assertEqual(dev.calls[-1][4], 57)

    def test_read_status_error_and_short_response(self):
        for response in (b'\x42' + bytes(56), bytes(56)):
            with patch.object(FakeUSB, 'ctrl_transfer', return_value=response):
                with self.assertRaises(RuntimeError):
                    exchange(FakeUSB())

    def test_status_only(self):
        dev = FakeUSB()
        exchange(dev)
        self.assertEqual(len(dev.calls), 1)

    def test_reject(self):
        with self.assertRaises(RuntimeError):
            exchange(FakeUSB(b"\x01"), command("off", -30, True))
        with self.assertRaises(ValueError):
            decode(bytes(56))


class FakeFilterUSB(FakeUSB):
    bcdDevice = 0x82

    def __init__(self, fault=None):
        super().__init__()
        self.fault = fault
        self.params = filter_command(9500, 0, -6)

    def ctrl_transfer(self, kind, req, value, index, payload, timeout):
        result = super().ctrl_transfer(kind, req, value, index, payload, timeout)
        if value == 19:
            if kind == 0x40:
                self.params = payload
                return 12
            return b'\1' if self.fault == 'reject' else b'\0'
        if value == 148:
            if self.fault == 'short': return b'\0'
            params = filter_command(9500,0,-6) if self.fault == 'mismatch' else self.params
            return b'\0' + params + struct.pack('<I', 7)
        if value == 146:
            result[4] = 3 if self.fault == 'active' else 1
            result[17:21] = result[21:25] = struct.pack('<I', 6)
        return result


if __name__ == "__main__":
    unittest.main()

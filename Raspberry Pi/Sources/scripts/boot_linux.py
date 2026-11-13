"""Explicit experimental V2 RAM boot on Pi. Never erases/programs QSPI."""
import argparse
import ctypes as c
import fcntl
import glob
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from spi_boot import boot, checked_image


class Message(c.Structure):
    _fields_ = [('addr', c.c_uint16), ('flags', c.c_uint16),
                ('length', c.c_uint16), ('buffer', c.c_void_p)]


class Transaction(c.Structure):
    _fields_ = [('messages', c.POINTER(Message)), ('count', c.c_uint32)]


class I2C:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_CLOEXEC)
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            fcntl.ioctl(self.fd, 0x0701, 0)  # I2C_RETRIES: no ambiguous write retry.
            fcntl.ioctl(self.fd, 0x0702, 25)  # I2C_TIMEOUT: 250 ms.
        except BaseException:
            os.close(self.fd)
            raise
        self.lib = c.CDLL(None, use_errno=True)
        self.lib.ioctl.restype = c.c_int

    def transfer(self, tx, count=0):
        source = c.create_string_buffer(bytes(tx))
        dest = c.create_string_buffer(count)
        messages = (Message*2)()
        messages[0] = Message(0x20, 0, len(tx), c.cast(source,c.c_void_p))
        n = 1
        if count:
            messages[1] = Message(0x20, 1, count, c.cast(dest,c.c_void_p))
            n = 2
        request = Transaction(messages,n)
        result = self.lib.ioctl(c.c_int(self.fd),c.c_ulong(0x0707),c.byref(request))
        if result != n:
            raise OSError(c.get_errno(), f'Incomplete PCAL I2C transfer {result}/{n}')
        return dest.raw

    def read(self, reg):
        return self.transfer([reg], 1)[0]

    def write(self, reg, value):
        self.transfer([reg, value])

    def close(self):
        os.close(self.fd)


def pinctrl(*args):
    return subprocess.check_output(['pinctrl', *map(str,args)], text=True, timeout=5)


def mode(line):
    match = re.match(r'\s*\d+:\s*(a[0-8]|ip|op|no)\b', line)
    if not match:
        raise ValueError('Unrecognized pinctrl output: '+line)
    return match[1]


def spi_function(line, signal):
    fields = [v.strip() for v in line.strip().split(',')]
    matches = [i for i,v in enumerate(fields[2:]) if v in signal]
    if len(matches)!=1 or matches[0]>8:
        raise ValueError('Cannot identify SPI0 pin function: '+line)
    return 'a'+str(matches[0])


def wait_pcal(read, timeout=20, sleep=time.sleep, now=time.monotonic):
    """Wait for readable registers before resetting XMOS or transmitting SPI.

    Only reads are retried. An ambiguous register write or SPI transfer is
    still a hard failure and must never be replayed automatically.
    """
    deadline = now() + timeout
    consecutive = attempts = 0
    last_error = None
    while True:
        attempts += 1
        try:
            read(6)
            read(2)
            consecutive += 1
            if consecutive == 3:
                print(json.dumps({'pcal_ready': True, 'read_attempts': attempts}), flush=True)
                return attempts
        except OSError as exc:
            last_error = exc
            consecutive = 0
            if attempts == 1:
                print(json.dumps({'pcal_ready': False, 'waiting_seconds': timeout,
                                  'error': str(exc)}), flush=True)
        if now() >= deadline:
            raise TimeoutError('PCAL not readable; no reset, SPI or audio attempted') from last_error
        sleep(.25)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--device', required=True)
    p.add_argument('--image', type=Path, required=True)
    p.add_argument('--confirm-v2-header', action='store_true', required=True)
    p.add_argument('--confirm-exclusive', action='store_true', required=True)
    a = p.parse_args()
    if sys.platform!='linux' or not re.fullmatch(r'/dev/i2c-\d+',a.device):
        p.error('Linux with an explicit I2C adapter is required')
    data = checked_image(a.image)
    import spidev
    if int(Path('/sys/module/spidev/parameters/bufsiz').read_text()) < 4096:
        raise RuntimeError('spidev buffer must allow one uninterrupted 4096-byte block')
    pcms=glob.glob('/dev/snd/pcm*')
    if not pcms:
        raise RuntimeError('Install/reboot the audio overlay first')
    busy=subprocess.run(['fuser',*pcms], capture_output=True, timeout=5)
    if busy.returncode!=1:
        raise RuntimeError('PCM in use or ownership check failed; stop audio clients first')
    saved={}
    for pin in range(18,22):
        line=pinctrl('get',pin)
        saved[pin]=mode(line)
        if not saved[pin].startswith('a') or not re.search(r'PCM_|I2S',line):
            raise RuntimeError('I2S pin mux not initialized by the expected overlay: '+line)
    functions={}
    for pin,names in ((9,('SPI0_MISO','SPI0_SIO[1]')),
                      (10,('SPI0_MOSI','SPI0_SIO[0]')),(11,('SPI0_SCLK',))):
        functions[pin]=spi_function(pinctrl('funcs',pin), names)
    bus=I2C(a.device)
    spi=spidev.SpiDev()
    opened=False
    try:
        pinctrl('set','4,18-21','ip','pn') # MCLK GPIO4 remains input permanently.
        spi.open(0,0);opened=True
        spi.mode=0;spi.max_speed_hz=1000000;spi.bits_per_word=8;spi.lsbfirst=False
        for pin,function in functions.items():
            pinctrl('set',pin,function,'pn')
        # Linux spi0 uses GPIO8 as chip select; idle inactive before the first block.
        pinctrl('set',8,'op','dh','pn')
        wait_pcal(bus.read)
        def transfer(block):
            result=spi.xfer2(list(block))
            if len(result)!=len(block):
                raise IOError('Short SPI block')
        report=boot(data,bus.read,bus.write,transfer)
        print(json.dumps(report),flush=True)
    finally:
        if opened:
            spi.close()
        try:
            pinctrl('set','8-11','ip','pn')
            for pin,function in saved.items():
                pinctrl('set',pin,function,'pn')
        finally:
            bus.close()


if __name__=='__main__':
    main()

"""V2 RAM-only boot, ported from accepted v2_spi_boot.cpp. No QSPI writes.

Caller must hold exclusive I2C ownership, stop all I2S streams and release their
pins, and provide SPI mode0/1MHz/8-bit/MSB-first. Bytes are bit-reversed here.
"""
import hashlib
import time

IMAGE_SHA256 = '152334171334c9608783985ee83ed4e3daad94385dd220bc4a5884b4eaa7dc50'
REVERSE = bytes(int(f'{v:08b}'[::-1], 2) for v in range(256))


def checked_image(path):
    data = path.read_bytes()
    if len(data) != 864256 or hashlib.sha256(data).hexdigest() != IMAGE_SHA256:
        raise ValueError('Not the pinned accepted HA/DOA/AEC SPI image')
    return data


def boot(data, read, write, transfer, sleep=time.sleep, cancelled=lambda: False):
    if len(data) != 864256 or hashlib.sha256(data).hexdigest() != IMAGE_SHA256:
        raise ValueError('Unverified XMOS image')
    config, output = read(6), read(2)
    output = (output & ~0x15) | 0x08
    config = (config & ~0x1d) | 0x80
    try:
        write(2, output)
        write(6, config)
        if read(0) & 0x11:
            raise RuntimeError('XMOS reset/external amp did not read low')
        sleep(.005)
        write(6, config | 1)
        sleep(.010)
        blocks = len(data) // 4096
        for index in range(blocks):
            if cancelled():
                raise InterruptedError('Cancelled RAM boot')
            if index == 1:
                sleep(.001)
            if index == 105:
                sleep(.005)
            if index + 1 == blocks:
                write(6, config | 0x0d)
            transfer(data[index*4096:(index+1)*4096].translate(REVERSE))
            sleep(.000005)
            if (index+1) % 32 == 0:
                sleep(.001)
    except BaseException:
        # Hold partial code in reset; preserve unrelated expander outputs.
        try:
            write(2, output)
            write(6, config)
        except Exception:
            pass
        raise
    sleep(.8)
    return {'sent_bytes': len(data), 'execution_proven': False, 'qspi_written': False}

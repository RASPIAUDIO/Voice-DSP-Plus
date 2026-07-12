#!/usr/bin/env python3
"""Temporarily boot PI_AI_MIC XVF3800 from Raspberry Pi SPI slave boot.

This does not program QSPI flash. It sets PCAL6408A P3 BOOT_SEL high, resets
the XVF3800 through P0 XVF_RST_N, streams a bit-reversed SPI boot image on
spidev0.0, then releases BOOT_SEL back to Hi-Z or low.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


I2C_BUS = 1
PCAL_ADDR = 0x20
PCAL_OUTPUT = 0x01
PCAL_CONFIG = 0x03

XVF_RST_N_BIT = 0
BOOT_SEL_BIT = 3
SPI_BLOCK_SIZE = 4096


def run(args: list[str], attempts: int = 5, delay_s: float = 0.1) -> str:
    last_exc: subprocess.CalledProcessError | None = None
    for attempt in range(1, attempts + 1):
        try:
            proc = subprocess.run(args, check=True, text=True, capture_output=True)
            return proc.stdout.strip()
        except subprocess.CalledProcessError as exc:
            last_exc = exc
            if attempt == attempts:
                break
            time.sleep(delay_s)
    assert last_exc is not None
    if last_exc.stderr:
        print(last_exc.stderr.strip(), file=sys.stderr)
    raise last_exc


def i2c_cmd(*args: str) -> list[str]:
    cmd = list(args)
    if os.geteuid() != 0:
        cmd.insert(0, "sudo")
    return cmd


def pcal_read(reg: int) -> int:
    out = run(i2c_cmd("i2cget", "-y", str(I2C_BUS), hex(PCAL_ADDR), hex(reg)))
    return int(out, 16)


def pcal_write(reg: int, value: int) -> None:
    run(i2c_cmd("i2cset", "-y", str(I2C_BUS), hex(PCAL_ADDR), hex(reg), hex(value & 0xFF)))


def set_output_bit(bit: int, value: int) -> None:
    reg = pcal_read(PCAL_OUTPUT)
    if value:
        reg |= 1 << bit
    else:
        reg &= ~(1 << bit)
    pcal_write(PCAL_OUTPUT, reg)


def set_direction_bit(bit: int, is_input: bool) -> None:
    reg = pcal_read(PCAL_CONFIG)
    if is_input:
        reg |= 1 << bit
    else:
        reg &= ~(1 << bit)
    pcal_write(PCAL_CONFIG, reg)


def set_pin(bit: int, value: int, enabled: bool = True) -> None:
    if enabled:
        set_direction_bit(bit, False)
    else:
        set_direction_bit(bit, True)
    set_output_bit(bit, value)


def decode_pcal() -> str:
    output = pcal_read(PCAL_OUTPUT)
    config = pcal_read(PCAL_CONFIG)
    return f"PCAL output=0x{output:02x} config=0x{config:02x}"


def prepare_for_boot(reset_release: str) -> None:
    print("Before boot:", decode_pcal())
    set_pin(XVF_RST_N_BIT, 0, enabled=True)
    set_pin(BOOT_SEL_BIT, 1, enabled=True)
    time.sleep(0.001)
    if reset_release == "drive-high":
        set_pin(XVF_RST_N_BIT, 1, enabled=True)
    else:
        set_pin(XVF_RST_N_BIT, 1, enabled=False)
    time.sleep(0.001)
    print("Boot mode:", decode_pcal())


def complete_boot(boot_sel_release: str) -> None:
    if boot_sel_release == "drive-low":
        set_pin(BOOT_SEL_BIT, 0, enabled=True)
    else:
        set_pin(BOOT_SEL_BIT, 0, enabled=False)
    print("After boot:", decode_pcal())


def bit_reverse_table() -> list[int]:
    return [int(f"{i:08b}"[::-1], 2) for i in range(256)]


def send_image(
    image: Path,
    spi_speed_hz: int,
    transfer_block_num: int | None,
    transfer_delay_ms: float,
    reload_spi_drivers: bool,
) -> None:
    try:
        import spidev
    except ImportError as exc:
        raise SystemExit("Missing Python module spidev. Install python3-spidev on the Raspberry Pi.") from exc

    if reload_spi_drivers:
        subprocess.run(["sudo", "rmmod", "spidev"], check=False)
        subprocess.run(["sudo", "rmmod", "spi_bcm2835"], check=False)
        subprocess.run(["sudo", "modprobe", "spidev"], check=True)
        subprocess.run(["sudo", "modprobe", "spi_bcm2835"], check=True)

    data = image.read_bytes()
    if len(data) % SPI_BLOCK_SIZE:
        raise SystemExit(f"{image} size {len(data)} is not a multiple of {SPI_BLOCK_SIZE}")

    reverse = bit_reverse_table()
    spi = spidev.SpiDev()
    spi.open(0, 0)
    spi.max_speed_hz = spi_speed_hz
    spi.mode = 0b00

    try:
        blocks = len(data) // SPI_BLOCK_SIZE
        print(f"Sending {len(data)} bytes in {blocks} blocks at {spi_speed_hz} Hz")
        for idx in range(blocks):
            if idx == 1:
                time.sleep(0.001)
            if transfer_block_num is not None and idx == transfer_block_num:
                time.sleep(transfer_delay_ms / 1000.0)
            block = data[idx * SPI_BLOCK_SIZE : (idx + 1) * SPI_BLOCK_SIZE]
            spi.xfer([reverse[b] for b in block])
            time.sleep(0.000005)
    finally:
        spi.close()
    print("SPI transfer complete")


def main() -> int:
    parser = argparse.ArgumentParser(description="Boot PI_AI_MIC XVF3800 from Raspberry Pi SPI")
    parser.add_argument("image", type=Path, help="*_spi_boot.bin image")
    parser.add_argument("--spi-speed-hz", type=int, default=5_000_000)
    parser.add_argument("--transfer-block-num", type=int, default=None)
    parser.add_argument("--transfer-delay-ms", type=float, default=5.0)
    parser.add_argument("--reset-release", choices=["hiz", "drive-high"], default="hiz")
    parser.add_argument("--boot-sel-release", choices=["hiz", "drive-low"], default="hiz")
    parser.add_argument("--reload-spi-drivers", action="store_true")
    parser.add_argument("--status-only", action="store_true")
    args = parser.parse_args()

    if args.status_only:
        print(decode_pcal())
        return 0
    if not args.image.is_file():
        raise SystemExit(f"Image not found: {args.image}")

    prepare_for_boot(args.reset_release)
    send_image(
        args.image,
        args.spi_speed_hz,
        args.transfer_block_num,
        args.transfer_delay_ms,
        args.reload_spi_drivers,
    )
    time.sleep(0.1)
    complete_boot(args.boot_sel_release)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Compare matched 32-bit PCM Voice DSP+ captures with AEC off and on."""

from __future__ import annotations

import argparse
import math
import sys
import wave
from array import array
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("off", type=Path, help="AEC-disabled reference capture")
    parser.add_argument("on", type=Path, help="AEC-enabled capture")
    parser.add_argument("--warmup-seconds", type=int, default=5)
    return parser.parse_args()


def load_pcm32(path: Path) -> tuple[array, int, int]:
    with wave.open(str(path), "rb") as source:
        if source.getsampwidth() != 4:
            raise ValueError(f"{path}: expected 32-bit PCM")
        channels = source.getnchannels()
        samplerate = source.getframerate()
        samples = array("i")
        samples.frombytes(source.readframes(source.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    return samples, samplerate, channels


def rms_dbfs(samples: array, start: int, stop: int) -> float:
    count = max(stop - start, 0)
    if count == 0:
        return float("-inf")
    power = sum(value * value for value in samples[start:stop]) / count
    rms = math.sqrt(power) / (2**31)
    return 20.0 * math.log10(max(rms, 1e-15))


def main() -> int:
    args = parse_args()
    off, off_rate, off_channels = load_pcm32(args.off)
    on, on_rate, on_channels = load_pcm32(args.on)
    if (off_rate, off_channels) != (on_rate, on_channels):
        raise ValueError("Capture formats do not match")

    samples_per_second = off_rate * off_channels
    seconds = min(len(off), len(on)) // samples_per_second
    warmup = min(max(args.warmup_seconds, 0), seconds)
    print(
        f"format={off_rate}Hz channels={off_channels} seconds={seconds} "
        f"warmup={warmup}s"
    )
    print("second,off_dbfs,on_dbfs,reduction_db")
    for second in range(seconds):
        start = second * samples_per_second
        stop = start + samples_per_second
        off_db = rms_dbfs(off, start, stop)
        on_db = rms_dbfs(on, start, stop)
        print(f"{second:02d},{off_db:.2f},{on_db:.2f},{off_db - on_db:.2f}")

    steady_start = warmup * samples_per_second
    steady_stop = seconds * samples_per_second
    off_steady = rms_dbfs(off, steady_start, steady_stop)
    on_steady = rms_dbfs(on, steady_start, steady_stop)
    print(
        f"steady_off_dbfs={off_steady:.2f}\n"
        f"steady_on_dbfs={on_steady:.2f}\n"
        f"steady_echo_reduction_db={off_steady - on_steady:.2f}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

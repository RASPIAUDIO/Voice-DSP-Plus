#!/usr/bin/env python3
"""Run the built-in Hey Jarvis model on the processed Voice DSP+ stream."""

from __future__ import annotations

import argparse
import math
import signal
import subprocess
import sys
import time
from array import array

from pyopen_wakeword import Model, OpenWakeWord, OpenWakeWordFeatures


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="plughw:2,0")
    parser.add_argument("--threshold", type=float, default=0.40)
    parser.add_argument("--cooldown", type=float, default=1.5)
    parser.add_argument("--status-interval", type=float, default=5.0)
    parser.add_argument("--reset-after-detection", action="store_true")
    return parser.parse_args()


def rms_s16le(audio: bytes) -> float:
    samples = array("h")
    samples.frombytes(audio)
    if sys.byteorder != "little":
        samples.byteswap()
    if not samples:
        return 0.0
    return math.sqrt(sum(sample * sample for sample in samples) / len(samples)) / 32768.0


def main() -> int:
    args = parse_args()
    command = [
        "arecord",
        "-q",
        "-D",
        args.device,
        "-f",
        "S16_LE",
        "-r",
        "16000",
        "-c",
        "1",
        "-t",
        "raw",
    ]

    wakeword = OpenWakeWord.from_builtin(Model.HEY_JARVIS)
    features = OpenWakeWordFeatures.from_builtin()
    capture = subprocess.Popen(command, stdout=subprocess.PIPE)
    stop_requested = False

    def stop_capture(_signum: int, _frame: object) -> None:
        nonlocal stop_requested
        stop_requested = True

    signal.signal(signal.SIGINT, stop_capture)
    signal.signal(signal.SIGTERM, stop_capture)

    print(
        f"READY model=hey_jarvis device={args.device} threshold={args.threshold:.2f} "
        f"cooldown={args.cooldown:.1f}s",
        flush=True,
    )
    last_detection = -args.cooldown
    last_status = time.monotonic()
    status_peak = 0.0
    status_rms = 0.0

    try:
        assert capture.stdout is not None
        while not stop_requested:
            audio = capture.stdout.read(2048)
            if not audio:
                break

            status_rms = max(status_rms, rms_s16le(audio))
            reset_requested = False
            for feature_batch in features.process_streaming(audio):
                for probability in wakeword.process_streaming(feature_batch):
                    probability = float(probability)
                    status_peak = max(status_peak, probability)
                    now = time.monotonic()
                    if (
                        probability >= args.threshold
                        and now - last_detection >= args.cooldown
                    ):
                        last_detection = now
                        print(
                            f"DETECTED model=hey_jarvis score={probability:.3f} "
                            f"rms={status_rms:.4f} time={time.strftime('%Y-%m-%dT%H:%M:%S')}",
                            flush=True,
                        )
                        reset_requested = args.reset_after_detection
                        if reset_requested:
                            break
                if reset_requested:
                    break

            if reset_requested:
                wakeword.reset()
                features.reset()

            now = time.monotonic()
            if now - last_status >= args.status_interval:
                print(
                    f"STATUS max_score={status_peak:.3f} max_rms={status_rms:.4f}",
                    flush=True,
                )
                last_status = now
                status_peak = 0.0
                status_rms = 0.0
    finally:
        capture.terminate()
        try:
            capture.wait(timeout=2)
        except subprocess.TimeoutExpired:
            capture.kill()
        wakeword.close()
        features.close()

    return 0 if stop_requested else (capture.returncode or 0)


if __name__ == "__main__":
    raise SystemExit(main())

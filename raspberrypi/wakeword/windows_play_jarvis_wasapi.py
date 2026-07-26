#!/usr/bin/env python3
"""Play the Jarvis reference on an explicit Windows WASAPI endpoint."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

import numpy as np
import sounddevice as sd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("audio", type=Path, nargs="?")
    parser.add_argument("--device", default="Conexant")
    parser.add_argument("--level", type=float, default=0.50)
    parser.add_argument("--samplerate", type=int, default=48000)
    parser.add_argument("--list-devices", action="store_true")
    return parser.parse_args()


def find_output(name_fragment: str) -> tuple[int, str, str]:
    matches: list[tuple[int, str, str]] = []
    for index, device in enumerate(sd.query_devices()):
        if int(device["max_output_channels"]) < 1:
            continue
        api = sd.query_hostapis(int(device["hostapi"]))["name"]
        if "WASAPI" not in api or name_fragment.casefold() not in device["name"].casefold():
            continue
        matches.append((index, device["name"], api))
    if not matches:
        raise RuntimeError(f"No Windows WASAPI output contains {name_fragment!r}")
    return matches[0]


def decode(path: Path, samplerate: int) -> np.ndarray:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg is required")
    result = subprocess.run(
        [
            ffmpeg,
            "-v",
            "error",
            "-i",
            str(path),
            "-f",
            "f32le",
            "-acodec",
            "pcm_f32le",
            "-ac",
            "2",
            "-ar",
            str(samplerate),
            "pipe:1",
        ],
        check=True,
        stdout=subprocess.PIPE,
    )
    return np.frombuffer(result.stdout, dtype="<f4").reshape(-1, 2)


def main() -> int:
    args = parse_args()
    if args.list_devices:
        for index, device in enumerate(sd.query_devices()):
            if int(device["max_output_channels"]) < 1:
                continue
            api = sd.query_hostapis(int(device["hostapi"]))["name"]
            print(f"{index}: {api} / {device['name']}")
        return 0
    if args.audio is None:
        raise SystemExit("audio is required unless --list-devices is used")
    if not args.audio.is_file():
        raise SystemExit(f"Audio file not found: {args.audio}")
    if not 0.0 < args.level <= 1.0:
        raise SystemExit("--level must be in (0, 1]")

    device_index, device_name, host_api = find_output(args.device)
    audio = np.clip(decode(args.audio, args.samplerate) * args.level, -1.0, 0.999969)
    print(
        f"PLAY device={device_index} name={device_name!r} api={host_api!r} "
        f"level={args.level:.2f} frames={len(audio)}",
        flush=True,
    )
    sd.play(audio, args.samplerate, device=device_index, blocking=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

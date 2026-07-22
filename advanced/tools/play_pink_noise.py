#!/usr/bin/env python3
"""Play repeatable pink noise through a selected audio output."""

from __future__ import annotations

import argparse
import time

import numpy as np
import sounddevice as sd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="Speakers (Realtek(R) Audio)")
    parser.add_argument("--host-api", default="Windows WASAPI")
    parser.add_argument("--dbfs", type=float, default=-36.0)
    parser.add_argument("--seconds", type=float, default=5.0)
    parser.add_argument("--samplerate", type=int, default=48000)
    parser.add_argument("--seed", type=int, default=3800)
    return parser.parse_args()


def find_output(name: str, host_api: str) -> int:
    fallback: list[int] = []
    for index, device in enumerate(sd.query_devices()):
        if device["max_output_channels"] < 1 or name.casefold() not in device["name"].casefold():
            continue
        fallback.append(index)
        api = sd.query_hostapis(device["hostapi"])["name"]
        if host_api.casefold() in api.casefold():
            return index
    if fallback:
        return fallback[0]
    raise RuntimeError(f"No output device contains {name!r}")


def pink_noise(frames: int, samplerate: int, dbfs: float, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    white = rng.standard_normal(frames)
    spectrum = np.fft.rfft(white)
    frequencies = np.fft.rfftfreq(frames, 1.0 / samplerate)
    spectrum[1:] /= np.sqrt(frequencies[1:])
    spectrum[0] = 0.0
    noise = np.fft.irfft(spectrum, n=frames).astype(np.float32)
    noise /= max(float(np.sqrt(np.mean(noise * noise))), 1e-12)
    noise *= 10.0 ** (dbfs / 20.0)
    fade_frames = min(int(0.1 * samplerate), frames // 2)
    fade = np.linspace(0.0, 1.0, fade_frames, dtype=np.float32)
    noise[:fade_frames] *= fade
    noise[-fade_frames:] *= fade[::-1]
    return noise


def main() -> int:
    args = parse_args()
    device = find_output(args.device, args.host_api)
    frames = int(args.seconds * args.samplerate)
    mono = pink_noise(frames, args.samplerate, args.dbfs, args.seed)
    stereo = np.repeat(mono[:, None], 2, axis=1)
    info = sd.query_devices(device)
    api = sd.query_hostapis(info["hostapi"])["name"]
    print(f"Playing {args.dbfs:.1f} dBFS pink noise on {device}: {api} / {info['name']}")
    sd.play(stereo, samplerate=args.samplerate, device=device, blocking=False)
    while sd.get_stream().active:
        time.sleep(0.05)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

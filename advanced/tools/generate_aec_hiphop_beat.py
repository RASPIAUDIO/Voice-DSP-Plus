#!/usr/bin/env python3
"""Generate a deterministic, bass-heavy beat for Voice DSP+ AEC tests."""

from __future__ import annotations

import argparse
import math
import wave
from pathlib import Path

import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("aec_hiphop_beat_16k.wav"))
    parser.add_argument("--samplerate", type=int, default=16000)
    parser.add_argument("--seconds", type=float, default=36.0)
    parser.add_argument("--bpm", type=float, default=84.0)
    parser.add_argument("--peak-dbfs", type=float, default=-9.0)
    return parser.parse_args()


def add_kick(track: np.ndarray, start: int, samplerate: int) -> None:
    frames = min(int(0.48 * samplerate), len(track) - start)
    if frames <= 0:
        return
    t = np.arange(frames, dtype=np.float64) / samplerate
    phase = 2.0 * math.pi * (
        42.0 * t + (78.0 - 42.0) * (1.0 - np.exp(-t / 0.025)) * 0.025
    )
    body = np.sin(phase) * np.exp(-t / 0.17)
    click = np.sin(2.0 * math.pi * 145.0 * t) * np.exp(-t / 0.018)
    track[start : start + frames] += 0.85 * body + 0.22 * click


def add_snare(
    track: np.ndarray, start: int, samplerate: int, rng: np.random.Generator
) -> None:
    frames = min(int(0.32 * samplerate), len(track) - start)
    if frames <= 0:
        return
    t = np.arange(frames, dtype=np.float64) / samplerate
    noise = rng.standard_normal(frames)
    bright = np.concatenate(([noise[0]], np.diff(noise)))
    tone = np.sin(2.0 * math.pi * 185.0 * t)
    envelope = np.exp(-t / 0.095)
    track[start : start + frames] += (0.34 * bright + 0.26 * tone) * envelope


def add_hat(
    track: np.ndarray, start: int, samplerate: int, rng: np.random.Generator
) -> None:
    frames = min(int(0.075 * samplerate), len(track) - start)
    if frames <= 0:
        return
    t = np.arange(frames, dtype=np.float64) / samplerate
    noise = rng.standard_normal(frames)
    bright = np.concatenate(([noise[0]], np.diff(noise)))
    track[start : start + frames] += 0.075 * bright * np.exp(-t / 0.022)


def add_bass_note(
    track: np.ndarray, start: int, frames: int, samplerate: int, frequency: float
) -> None:
    frames = min(frames, len(track) - start)
    if frames <= 0:
        return
    t = np.arange(frames, dtype=np.float64) / samplerate
    attack = np.minimum(t / 0.018, 1.0)
    release = np.minimum((frames / samplerate - t) / 0.10, 1.0)
    envelope = np.clip(attack * release, 0.0, 1.0)
    bass = (
        np.sin(2.0 * math.pi * frequency * t)
        + 0.58 * np.sin(2.0 * math.pi * 2.0 * frequency * t)
        + 0.18 * np.sin(2.0 * math.pi * 3.0 * frequency * t)
    )
    track[start : start + frames] += 0.32 * bass * envelope


def make_beat(samplerate: int, seconds: float, bpm: float) -> np.ndarray:
    frames = int(round(seconds * samplerate))
    track = np.zeros(frames, dtype=np.float64)
    rng = np.random.default_rng(3800)
    beat_frames = samplerate * 60.0 / bpm
    eighth_frames = beat_frames / 2.0
    bass_notes = (55.0, 55.0, 65.406, 49.0, 55.0, 73.416, 65.406, 49.0)

    eighth = 0
    while int(round(eighth * eighth_frames)) < frames:
        start = int(round(eighth * eighth_frames))
        add_hat(track, start, samplerate, rng)
        if eighth % 2 == 0:
            beat = eighth // 2
            if beat % 4 in (0, 2) or beat % 8 == 7:
                add_kick(track, start, samplerate)
            if beat % 4 in (1, 3):
                add_snare(track, start, samplerate, rng)
            note = bass_notes[beat % len(bass_notes)]
            add_bass_note(track, start, int(0.92 * beat_frames), samplerate, note)
        eighth += 1

    # A quiet chord stab adds midrange content that makes AEC artifacts audible.
    bar_frames = int(round(4.0 * beat_frames))
    for start in range(0, frames, bar_frames):
        chord_frames = min(int(0.42 * samplerate), frames - start)
        t = np.arange(chord_frames, dtype=np.float64) / samplerate
        chord = sum(
            np.sin(2.0 * math.pi * frequency * t)
            for frequency in (220.0, 261.626, 329.628)
        ) / 3.0
        track[start : start + chord_frames] += 0.10 * chord * np.exp(-t / 0.18)

    fade_frames = min(int(0.05 * samplerate), frames // 2)
    fade = np.linspace(0.0, 1.0, fade_frames, dtype=np.float64)
    track[:fade_frames] *= fade
    track[-fade_frames:] *= fade[::-1]
    return track.astype(np.float32)


def write_wav(path: Path, audio: np.ndarray, samplerate: int, peak_dbfs: float) -> None:
    peak = max(float(np.max(np.abs(audio))), 1e-12)
    target_peak = 10.0 ** (peak_dbfs / 20.0)
    audio = np.clip(audio * (target_peak / peak), -1.0, 0.999969)
    stereo = np.column_stack((audio, audio))
    pcm = (stereo * 32767.0).astype("<i2")
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(samplerate)
        output.writeframes(pcm.tobytes())
    rms = float(np.sqrt(np.mean(np.square(audio.astype(np.float64)))))
    print(
        f"Wrote {path}: {samplerate} Hz, {len(audio) / samplerate:.1f} s, "
        f"peak={peak_dbfs:.1f} dBFS, rms={20.0 * math.log10(max(rms, 1e-12)):.1f} dBFS"
    )


def main() -> int:
    args = parse_args()
    audio = make_beat(args.samplerate, args.seconds, args.bpm)
    write_wav(args.output, audio, args.samplerate, args.peak_dbfs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

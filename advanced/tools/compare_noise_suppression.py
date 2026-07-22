#!/usr/bin/env python3
"""Record Voice DSP+ and a PC microphone while playing controlled noise."""

from __future__ import annotations

import argparse
import json
import math
import threading
import time
import wave
from pathlib import Path

try:
    import numpy as np
    import sounddevice as sd
except ImportError as exc:
    raise SystemExit(
        "Install dependencies with: python -m pip install numpy sounddevice"
    ) from exc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Record Voice DSP+ and the PC microphone in parallel while pink "
            "noise is played through the PC speakers."
        )
    )
    parser.add_argument("--list-devices", action="store_true")
    parser.add_argument("--samplerate", type=int, default=48000)
    parser.add_argument("--voice-device", default="PI AI MIC Assistant Auto")
    parser.add_argument(
        "--reference-device", default="Internal Microphone (Conexant SmartAudio HD)"
    )
    parser.add_argument(
        "--speaker-device", default="Speakers / Headphones (Conexant SmartAudio HD)"
    )
    parser.add_argument("--host-api", default="Windows WASAPI")
    parser.add_argument("--noise-dbfs", type=float, default=-24.0)
    parser.add_argument("--settle-seconds", type=float, default=1.5)
    parser.add_argument("--noise-only-seconds", type=float, default=5.0)
    parser.add_argument("--speech-noise-seconds", type=float, default=12.0)
    parser.add_argument("--speech-clean-seconds", type=float, default=8.0)
    parser.add_argument("--output-dir", type=Path, default=Path("noise_test"))
    return parser.parse_args()


def host_api_name(device: dict) -> str:
    return str(sd.query_hostapis(device["hostapi"])["name"])


def find_device(name: str, host_api: str, *, input_device: bool) -> tuple[int, dict]:
    candidates: list[tuple[int, dict]] = []
    fallback: list[tuple[int, dict]] = []
    channel_key = "max_input_channels" if input_device else "max_output_channels"
    for index, device in enumerate(sd.query_devices()):
        if int(device[channel_key]) < 1 or name.casefold() not in device["name"].casefold():
            continue
        fallback.append((index, device))
        if host_api.casefold() in host_api_name(device).casefold():
            candidates.append((index, device))
    matches = candidates or fallback
    if not matches:
        kind = "input" if input_device else "output"
        raise RuntimeError(f"No {kind} device contains {name!r}")
    return matches[0]


def pink_noise(frames: int, samplerate: int, dbfs: float, seed: int = 3800) -> np.ndarray:
    rng = np.random.default_rng(seed)
    white = rng.standard_normal(frames)
    spectrum = np.fft.rfft(white)
    frequencies = np.fft.rfftfreq(frames, 1.0 / samplerate)
    spectrum[1:] /= np.sqrt(frequencies[1:])
    spectrum[0] = 0.0
    noise = np.fft.irfft(spectrum, n=frames).astype(np.float32)
    noise /= max(float(np.sqrt(np.mean(noise * noise))), 1e-12)
    noise *= 10.0 ** (dbfs / 20.0)
    fade_frames = min(int(0.05 * samplerate), frames // 2)
    if fade_frames:
        fade = np.linspace(0.0, 1.0, fade_frames, dtype=np.float32)
        noise[:fade_frames] *= fade
        noise[-fade_frames:] *= fade[::-1]
    return noise


def sync_tone(samplerate: int, seconds: float = 0.25) -> np.ndarray:
    frames = int(seconds * samplerate)
    t = np.arange(frames, dtype=np.float32) / samplerate
    envelope = np.sin(np.linspace(0.0, math.pi, frames, dtype=np.float32)) ** 2
    return (0.08 * np.sin(2.0 * math.pi * 1000.0 * t) * envelope).astype(np.float32)


def cue_tones(samplerate: int, count: int) -> np.ndarray:
    tone_frames = int(0.12 * samplerate)
    gap_frames = int(0.12 * samplerate)
    t = np.arange(tone_frames, dtype=np.float32) / samplerate
    envelope = np.sin(
        np.linspace(0.0, math.pi, tone_frames, dtype=np.float32)
    ) ** 2
    tone = 0.08 * np.sin(2.0 * math.pi * 1200.0 * t) * envelope
    gap = np.zeros(gap_frames, dtype=np.float32)
    return np.concatenate([tone, gap] * count).astype(np.float32)


class Capture:
    def __init__(self, device: int, channels: int, samplerate: int) -> None:
        self._chunks: list[np.ndarray] = []
        self._frames = 0
        self._lock = threading.Lock()
        self.stream = sd.InputStream(
            device=device,
            channels=channels,
            samplerate=samplerate,
            dtype="float32",
            callback=self._callback,
        )

    def _callback(self, indata: np.ndarray, frames: int, _time: object, status: object) -> None:
        if status:
            print(f"Input warning: {status}")
        with self._lock:
            self._chunks.append(indata.copy())
            self._frames += frames

    def frame_count(self) -> int:
        with self._lock:
            return self._frames

    def data(self) -> np.ndarray:
        with self._lock:
            return np.concatenate(self._chunks, axis=0)


def write_wav(path: Path, data: np.ndarray, samplerate: int) -> None:
    if data.ndim == 1:
        data = data[:, None]
    pcm = (np.clip(data, -1.0, 0.999969) * 32767.0).astype("<i2")
    with wave.open(str(path), "wb") as output:
        output.setnchannels(pcm.shape[1])
        output.setsampwidth(2)
        output.setframerate(samplerate)
        output.writeframes(pcm.tobytes())


def dbfs(data: np.ndarray) -> float:
    rms = float(np.sqrt(np.mean(np.square(data.astype(np.float64)))))
    return 20.0 * math.log10(max(rms, 1e-12))


def trim_capture(data: np.ndarray, start: int, frames: int) -> np.ndarray:
    result = data[start : start + frames]
    if len(result) < frames:
        result = np.pad(result, ((0, frames - len(result)), (0, 0)))
    return result


def phase_metrics(
    mono: np.ndarray, samplerate: int, phase_frames: dict[str, tuple[int, int]]
) -> dict[str, float]:
    levels = {
        name: dbfs(mono[start:end]) for name, (start, end) in phase_frames.items()
    }
    levels["clean_speech_to_noise_contrast_db"] = (
        levels["speech_clean_dbfs"] - levels["noise_only_dbfs"]
    )
    return levels


def main() -> int:
    args = parse_args()
    if args.list_devices:
        print(sd.query_devices())
        return 0

    voice_index, voice_info = find_device(
        args.voice_device, args.host_api, input_device=True
    )
    reference_index, reference_info = find_device(
        args.reference_device, args.host_api, input_device=True
    )
    speaker_index, speaker_info = find_device(
        args.speaker_device, args.host_api, input_device=False
    )
    voice_channels = min(2, int(voice_info["max_input_channels"]))
    reference_channels = min(2, int(reference_info["max_input_channels"]))
    output_channels = min(2, int(speaker_info["max_output_channels"]))

    print(f"Voice DSP+: {voice_index} / {voice_info['name']} / {host_api_name(voice_info)}")
    print(
        f"Reference: {reference_index} / {reference_info['name']} / "
        f"{host_api_name(reference_info)}"
    )
    print(
        f"Noise output: {speaker_index} / {speaker_info['name']} / "
        f"{host_api_name(speaker_info)}"
    )
    print("Set the Conexant speaker volume low before continuing.")
    for remaining in range(3, 0, -1):
        print(f"Starting in {remaining}...")
        time.sleep(1.0)

    sr = args.samplerate
    settle_frames = int(args.settle_seconds * sr)
    noise_only_frames = int(args.noise_only_seconds * sr)
    speech_noise_frames = int(args.speech_noise_seconds * sr)
    speech_clean_frames = int(args.speech_clean_seconds * sr)
    tone = sync_tone(sr)
    speech_cue = cue_tones(sr, 2)
    clean_cue = cue_tones(sr, 1)
    noise = pink_noise(
        noise_only_frames + len(speech_cue) + speech_noise_frames,
        sr,
        args.noise_dbfs,
    )
    silence_settle = np.zeros(settle_frames, dtype=np.float32)
    silence_clean = np.zeros(speech_clean_frames, dtype=np.float32)
    noise_cue_start = noise_only_frames
    noise_speech_start = noise_cue_start + len(speech_cue)
    noise_with_cue = noise.copy()
    noise_with_cue[noise_cue_start:noise_speech_start] += speech_cue
    protocol = np.concatenate(
        (tone, silence_settle, noise_with_cue, clean_cue, silence_clean)
    )
    protocol_frames = len(protocol)

    voice_capture = Capture(voice_index, voice_channels, sr)
    reference_capture = Capture(reference_index, reference_channels, sr)
    output_stream = sd.OutputStream(
        device=speaker_index,
        channels=output_channels,
        samplerate=sr,
        dtype="float32",
    )
    voice_capture.stream.start()
    reference_capture.stream.start()
    output_stream.start()
    time.sleep(0.5)
    voice_start = voice_capture.frame_count()
    reference_start = reference_capture.frame_count()

    def play(samples: np.ndarray) -> None:
        output_stream.write(np.repeat(samples[:, None], output_channels, axis=1))

    print("SYNC/SETTLE: remain silent.")
    play(tone)
    play(silence_settle)
    print("NOISE ONLY: remain silent.")
    play(noise[:noise_only_frames])
    print("TWO BEEPS: start speaking after the cue.")
    play(noise_with_cue[noise_cue_start:noise_speech_start])
    print("NOISE + SPEECH: speak naturally and keep speaking.")
    play(noise_with_cue[noise_speech_start:])
    print("ONE BEEP: noise stopped; keep speaking naturally after the cue.")
    play(clean_cue)
    play(silence_clean)
    time.sleep(0.75)

    output_stream.stop()
    voice_capture.stream.stop()
    reference_capture.stream.stop()
    output_stream.close()
    voice_capture.stream.close()
    reference_capture.stream.close()

    voice = trim_capture(voice_capture.data(), voice_start, protocol_frames)
    reference = trim_capture(reference_capture.data(), reference_start, protocol_frames)
    voice_mono = voice[:, 0]
    reference_mono = reference[:, 0]

    offset = len(tone) + settle_frames
    speech_noise_start = offset + noise_only_frames + len(speech_cue)
    speech_clean_start = speech_noise_start + speech_noise_frames + len(clean_cue)
    phase_frames = {
        "noise_only_dbfs": (offset, offset + noise_only_frames),
        "speech_noise_dbfs": (
            speech_noise_start,
            speech_noise_start + speech_noise_frames,
        ),
        "speech_clean_dbfs": (speech_clean_start, protocol_frames),
    }
    voice_metrics = phase_metrics(voice_mono, sr, phase_frames)
    reference_metrics = phase_metrics(reference_mono, sr, phase_frames)
    advantage = (
        voice_metrics["clean_speech_to_noise_contrast_db"]
        - reference_metrics["clean_speech_to_noise_contrast_db"]
    )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_wav(args.output_dir / "voice_dsp_plus.wav", voice, sr)
    write_wav(args.output_dir / "conexant.wav", reference, sr)
    write_wav(
        args.output_dir / "comparison_left_voice_dsp_right_conexant.wav",
        np.column_stack((voice_mono, reference_mono)),
        sr,
    )
    gap = np.zeros(sr, dtype=np.float32)
    write_wav(
        args.output_dir / "comparison_ab.wav",
        np.concatenate((voice_mono, gap, reference_mono)),
        sr,
    )
    report = {
        "samplerate": sr,
        "protocol_seconds": protocol_frames / sr,
        "noise_level_dbfs": args.noise_dbfs,
        "voice_dsp_plus": voice_metrics,
        "conexant": reference_metrics,
        "voice_dsp_contrast_advantage_db": advantage,
        "note": (
            "Contrast is clean-speech RMS minus noise-only RMS. It is a useful "
            "demo proxy, not a laboratory SNR measurement."
        ),
    }
    (args.output_dir / "metrics.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )
    print(json.dumps(report, indent=2))
    print(f"Files written to: {args.output_dir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

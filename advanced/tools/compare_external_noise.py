#!/usr/bin/env python3
"""Compare Voice DSP+ and a PC microphone with repeatable voice playback."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import time
from pathlib import Path

import numpy as np
import sounddevice as sd

from compare_noise_suppression import (
    Capture,
    cue_tones,
    dbfs,
    find_device,
    host_api_name,
    sync_tone,
    trim_capture,
    write_wav,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Play a repeatable voice sample through the PC speakers while "
            "recording Voice DSP+ and the PC microphone in parallel."
        )
    )
    parser.add_argument(
        "voice_file",
        type=Path,
        nargs="?",
        default=(
            Path.home()
            / "Downloads"
            / "Emmanuel Macron Im not a naive optimist BBC News.mp3"
        ),
    )
    parser.add_argument("--samplerate", type=int, default=48000)
    parser.add_argument("--voice-start", type=float, default=5.0)
    parser.add_argument("--voice-seconds", type=float, default=25.0)
    parser.add_argument("--live-speech", action="store_true")
    parser.add_argument("--live-speech-seconds", type=float, default=20.0)
    parser.add_argument("--background-seconds", type=float, default=5.0)
    parser.add_argument("--tail-seconds", type=float, default=5.0)
    parser.add_argument("--voice-device", default="PI AI MIC Assistant Auto")
    parser.add_argument(
        "--reference-device", default="Internal Microphone (Conexant SmartAudio HD)"
    )
    parser.add_argument(
        "--speaker-device", default="Speakers / Headphones (Conexant SmartAudio HD)"
    )
    parser.add_argument("--host-api", default="Windows WASAPI")
    parser.add_argument("--output-dir", type=Path, default=Path("external_noise_test"))
    return parser.parse_args()


def decode_audio(path: Path, samplerate: int, start: float, seconds: float) -> np.ndarray:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg is required to decode the voice file")
    command = [
        ffmpeg,
        "-v",
        "error",
        "-ss",
        str(start),
        "-i",
        str(path),
        "-t",
        str(seconds),
        "-f",
        "f32le",
        "-acodec",
        "pcm_f32le",
        "-ac",
        "1",
        "-ar",
        str(samplerate),
        "pipe:1",
    ]
    result = subprocess.run(command, check=True, stdout=subprocess.PIPE)
    audio = np.frombuffer(result.stdout, dtype="<f4").copy()
    if not len(audio):
        raise RuntimeError(f"No audio decoded from {path}")
    fade_frames = min(int(0.03 * samplerate), len(audio) // 2)
    if fade_frames:
        fade = np.linspace(0.0, 1.0, fade_frames, dtype=np.float32)
        audio[:fade_frames] *= fade
        audio[-fade_frames:] *= fade[::-1]
    return audio


def active_rms(data: np.ndarray, samplerate: int) -> float:
    block_frames = max(1, int(0.1 * samplerate))
    usable = len(data) - (len(data) % block_frames)
    blocks = data[:usable].reshape(-1, block_frames)
    block_rms = np.sqrt(np.mean(np.square(blocks.astype(np.float64)), axis=1))
    threshold = np.percentile(block_rms, 60.0)
    active = blocks[block_rms >= threshold]
    return float(np.sqrt(np.mean(np.square(active.astype(np.float64)))))


def level_match(
    data: np.ndarray,
    speech: np.ndarray,
    samplerate: int,
    target_dbfs: float = -20.0,
) -> tuple[np.ndarray, float]:
    source_rms = max(active_rms(speech, samplerate), 1e-12)
    target_rms = 10.0 ** (target_dbfs / 20.0)
    gain = min(target_rms / source_rms, 10.0 ** (45.0 / 20.0))
    return np.clip(data * gain, -1.0, 0.999969), 20.0 * math.log10(gain)


def microphone_metrics(
    mono: np.ndarray,
    samplerate: int,
    background_ranges: list[tuple[int, int]],
    speech_range: tuple[int, int],
) -> dict[str, float]:
    background = np.concatenate([mono[start:end] for start, end in background_ranges])
    speech = mono[speech_range[0] : speech_range[1]]
    background_level = dbfs(background)
    speech_level = 20.0 * math.log10(max(active_rms(speech, samplerate), 1e-12))
    return {
        "background_dbfs": background_level,
        "active_voice_plus_background_dbfs": speech_level,
        "voice_to_background_contrast_db": speech_level - background_level,
        "raw_peak_dbfs": 20.0 * math.log10(max(float(np.max(np.abs(mono))), 1e-12)),
    }


def main() -> int:
    args = parse_args()
    if not args.voice_file.is_file():
        raise SystemExit(f"Voice file not found: {args.voice_file}")

    sr = args.samplerate
    if args.live_speech:
        voice_source = np.zeros(int(args.live_speech_seconds * sr), dtype=np.float32)
        voice_source_description = "live speech after two beeps"
    else:
        voice_source = decode_audio(
            args.voice_file, sr, args.voice_start, args.voice_seconds
        )
        voice_source_description = str(args.voice_file)
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
        f"Voice output: {speaker_index} / {speaker_info['name']} / "
        f"{host_api_name(speaker_info)}"
    )
    print(f"Voice source: {voice_source_description}")
    print("Keep the distant background-noise source running for the complete test.")
    if args.live_speech:
        print("Speak after TWO beeps and stop after ONE beep.")
    for remaining in range(3, 0, -1):
        print(f"Starting in {remaining}...")
        time.sleep(1.0)

    tone = sync_tone(sr)
    settle = np.zeros(int(1.5 * sr), dtype=np.float32)
    background = np.zeros(int(args.background_seconds * sr), dtype=np.float32)
    cue = 2.5 * cue_tones(sr, 2)
    stop_cue = 2.5 * cue_tones(sr, 1)
    tail = np.zeros(int(args.tail_seconds * sr), dtype=np.float32)
    protocol = np.concatenate(
        (tone, settle, background, cue, voice_source, stop_cue, tail)
    )

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
    voice_capture_start = voice_capture.frame_count()
    reference_capture_start = reference_capture.frame_count()

    def play(samples: np.ndarray) -> None:
        output_stream.write(np.repeat(samples[:, None], output_channels, axis=1))

    print("SYNC/SETTLE: background source on; no voice playback yet.")
    play(tone)
    play(settle)
    print("BACKGROUND ONLY: measuring the distant noise.")
    play(background)
    if args.live_speech:
        print("TWO BEEPS: start speaking now.")
    else:
        print("TWO BEEPS: repeatable voice playback starts after the cue.")
    play(cue)
    play(voice_source)
    print("ONE BEEP: stop speaking now.")
    play(stop_cue)
    print("VOICE STOPPED: measuring the distant background again.")
    play(tail)
    time.sleep(0.75)

    output_stream.stop()
    voice_capture.stream.stop()
    reference_capture.stream.stop()
    output_stream.close()
    voice_capture.stream.close()
    reference_capture.stream.close()

    frames = len(protocol)
    voice = trim_capture(voice_capture.data(), voice_capture_start, frames)
    reference = trim_capture(reference_capture.data(), reference_capture_start, frames)
    voice_mono = voice[:, 0]
    reference_mono = reference[:, 0]

    background_start = len(tone) + len(settle)
    background_end = background_start + len(background)
    speech_start = background_end + len(cue)
    speech_end = speech_start + len(voice_source)
    tail_start = speech_end + len(stop_cue)
    margin = int(0.25 * sr)
    background_ranges = [
        (background_start + margin, background_end - margin),
        (tail_start + margin, frames - margin),
    ]
    speech_range = (speech_start + margin, speech_end - margin)

    voice_metrics = microphone_metrics(
        voice_mono, sr, background_ranges, speech_range
    )
    reference_metrics = microphone_metrics(
        reference_mono, sr, background_ranges, speech_range
    )
    voice_matched, voice_gain_db = level_match(
        voice_mono, voice_mono[speech_range[0] : speech_range[1]], sr
    )
    reference_matched, reference_gain_db = level_match(
        reference_mono, reference_mono[speech_range[0] : speech_range[1]], sr
    )
    voice_metrics["listening_match_gain_db"] = voice_gain_db
    reference_metrics["listening_match_gain_db"] = reference_gain_db

    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_wav(args.output_dir / "voice_dsp_plus_raw.wav", voice, sr)
    write_wav(args.output_dir / "conexant_raw.wav", reference, sr)
    if not args.live_speech:
        write_wav(args.output_dir / "voice_source_48k.wav", voice_source, sr)
    write_wav(
        args.output_dir / "comparison_raw_left_voice_dsp_right_conexant.wav",
        np.column_stack((voice_mono, reference_mono)),
        sr,
    )
    write_wav(
        args.output_dir / "comparison_level_matched_left_voice_dsp_right_conexant.wav",
        np.column_stack((voice_matched, reference_matched)),
        sr,
    )
    gap = np.zeros(sr, dtype=np.float32)
    write_wav(
        args.output_dir / "comparison_ab_level_matched.wav",
        np.concatenate((voice_matched, gap, reference_matched)),
        sr,
    )

    report = {
        "samplerate": sr,
        "voice_source": voice_source_description,
        "voice_source_start_seconds": None if args.live_speech else args.voice_start,
        "voice_source_duration_seconds": len(voice_source) / sr,
        "voice_dsp_plus": voice_metrics,
        "conexant": reference_metrics,
        "voice_dsp_contrast_advantage_db": (
            voice_metrics["voice_to_background_contrast_db"]
            - reference_metrics["voice_to_background_contrast_db"]
        ),
        "note": (
            "Contrast compares active voice-plus-background with background-only. "
            "Level-matched files apply a constant gain and preserve each input's "
            "voice-to-background ratio."
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

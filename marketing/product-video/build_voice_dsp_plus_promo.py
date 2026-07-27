from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import subprocess
import urllib.request
import wave
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

import numpy as np


HERE = Path(__file__).resolve().parent
CACHE_DIR = HERE / ".cache"
BUILD_DIR = HERE / ".build"
DEFAULT_OUTPUT = HERE / "voice-dsp-plus-overview-90s.mp4"
DEFAULT_POSTER = HERE / "voice-dsp-plus-overview-90s-poster.jpg"
DEFAULT_MANIFEST = HERE / "manifest.json"
WIDTH = 1920
HEIGHT = 1080
FPS = 30
TOTAL_DURATION = 90.0

ASSETS = {
    "logo": {
        "url": "https://raspiaudio.com/wp-content/uploads/2020/08/Logo_raspiaudio_V3.png",
        "filename": "raspiaudio-logo.png",
        "kind": "Official RASPIAUDIO logo",
    },
    "pcb": {
        "url": "https://raspiaudio.com/wp-content/uploads/2026/06/pcb-top-1200x1331.png",
        "filename": "voice-dsp-plus-pcb.png",
        "kind": "Official Voice DSP+ product photo",
    },
    "usb": {
        "url": "https://raspiaudio.com/wp-content/uploads/2026/06/side_connection-1200x1027.png",
        "filename": "voice-dsp-plus-usb.png",
        "kind": "Official Voice DSP+ USB connection photo",
    },
    "web_updater": {
        "url": "https://raspiaudio.com/voicedsp/webflasher/",
        "filename": "source-assets/web-updater.png",
        "kind": "Voice DSP+ Web Updater screenshot captured from the public page",
        "local": HERE / "source-assets" / "web-updater.png",
    },
    "speaker": {
        "url": "https://raspiaudio.com/wp-content/uploads/2026/06/with-speaker-1200x608.png",
        "filename": "voice-dsp-plus-speaker.png",
        "kind": "Official Voice DSP+ speaker photo",
    },
    "features": {
        "url": "https://raspiaudio.com/wp-content/uploads/2026/07/voicedsp-hardware-features-20260720.png",
        "filename": "voice-dsp-plus-features.png",
        "kind": "Official Voice DSP+ feature diagram",
    },
    "geometry": {
        "url": "https://raspiaudio.com/wp-content/uploads/2026/06/geometry-1200x863.png",
        "filename": "voice-dsp-plus-geometries.png",
        "kind": "Official Voice DSP+ microphone geometry diagram",
    },
    "pi": {
        "url": "https://raspiaudio.com/wp-content/uploads/2026/06/PI-modes-1200x861.png",
        "filename": "voice-dsp-plus-raspberry-pi.png",
        "kind": "Official Voice DSP+ Raspberry Pi mode diagram",
    },
    "applications": {
        "url": "https://raspiaudio.com/wp-content/uploads/2026/06/PI-applications-768x703.png",
        "filename": "voice-dsp-plus-applications.png",
        "kind": "Official Voice DSP+ application photo",
    },
    "aec_demo": {
        "url": "https://raspiaudio.com/voicedsp/media/voice-dsp-plus-aec-off-on-spectrum.mp4",
        "filename": "voice-dsp-plus-aec-demo.mp4",
        "kind": "Validated RASPIAUDIO AEC demo",
    },
    "noise_demo": {
        "url": "https://raspiaudio.com/voicedsp/media/voice-dsp-plus-noise-suppression-spectrum.mp4",
        "filename": "voice-dsp-plus-noise-demo.mp4",
        "kind": "Validated RASPIAUDIO noise suppression demo",
    },
    "wake_demo": {
        "url": "https://raspiaudio.com/voicedsp/media/voice-dsp-plus-hey-jarvis-far-field.mp4",
        "filename": "voice-dsp-plus-wake-word-demo.mp4",
        "kind": "Validated RASPIAUDIO far-field wake-word demo",
        "preferred_local": HERE.parents[2]
        / "production-deploy"
        / "voicedsp-webflasher"
        / "source"
        / "media"
        / "voice-dsp-plus-hey-jarvis-far-field.mp4",
    },
}


@dataclass(frozen=True)
class StillScene:
    key: str
    duration: float
    asset: str
    title: str
    subtitle: str
    bullets: tuple[str, ...]
    theme: str = "dark"
    card: bool = False
    rotate_180: bool = False
    logo: bool = False


STILL_SCENES = (
    StillScene(
        "01-hero",
        7.0,
        "pcb",
        "A COMPLETE VOICE INTERFACE",
        "FOR AI PRODUCTS",
        ("Four microphones", "Voice processing", "Playback on one board"),
        theme="light",
        rotate_180=True,
        logo=True,
    ),
    StillScene(
        "02-usb",
        11.0,
        "web_updater",
        "USB-C. PLUG IN. START TALKING.",
        "Microphone + speaker, 48 kHz, no product-specific driver.",
        ("Windows, macOS and Linux", "Two processed mic channels", "Stereo playback", "Web Updater in Chrome or Edge"),
        card=True,
    ),
    StillScene(
        "03-io",
        11.0,
        "speaker",
        "CAPTURE AND PLAYBACK. TOGETHER.",
        "A complete two-way audio path for interactive products.",
        ("4 digital microphones", "Onboard speaker", "11 W passive-speaker output", "Auto-switching stereo headphones"),
        theme="light",
    ),
    StillScene(
        "04-dsp",
        10.0,
        "features",
        "VOICE PROCESSING ON THE BOARD",
        "A dedicated XMOS XVF3800 handles the audio front end.",
        ("Beamforming", "AEC", "Noise suppression", "AGC + limiter", "Direction of arrival"),
        card=True,
    ),
    StillScene(
        "05-geometry",
        9.0,
        "geometry",
        "ONE BOARD. TWO GEOMETRIES.",
        "Choose the microphone field that matches the product.",
        ("SQUARE for 360-degree awareness", "LINE for a 180-degree front field"),
        theme="light",
        card=True,
    ),
    StillScene(
        "06-pi",
        10.0,
        "pi",
        "RASPBERRY PI MODE",
        "Use the 40-pin header when the application needs deeper control.",
        ("Direct I2S audio", "I2C host control", "Advanced DSP settings", "Pi CPU stays free for the AI app"),
        card=True,
    ),
    StillScene(
        "07-applications",
        8.0,
        "applications",
        "BUILT FOR REAL VOICE PRODUCTS",
        "From a fast USB prototype to a complete Raspberry Pi appliance.",
        ("AI assistants", "Robots", "Conferencing", "Intercoms", "Kiosks + interactive products"),
        theme="light",
        card=True,
    ),
)

DEMO_SCENES = (
    ("08-aec", 7.0, "aec_demo", 2.5),
    ("09-noise", 7.0, "noise_demo", 4.0),
    ("10-wake", 7.0, "wake_demo", 8.0),
)


def run(command: list[str]) -> None:
    print(" ".join(command))
    subprocess.run(command, check=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download_assets(refresh: bool) -> dict[str, Path]:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    paths: dict[str, Path] = {}
    for key, info in ASSETS.items():
        if "local" in info:
            target = Path(info["local"])
            if not target.is_file():
                raise FileNotFoundError(target)
            paths[key] = target
            continue
        preferred_local = Path(info["preferred_local"]) if "preferred_local" in info else None
        if preferred_local is not None and preferred_local.is_file():
            paths[key] = preferred_local
            continue
        target = CACHE_DIR / info["filename"]
        if refresh or not target.is_file() or target.stat().st_size < 1024:
            request = urllib.request.Request(
                info["url"], headers={"User-Agent": "RASPIAUDIO-Voice-DSP-Plus-video-builder/1.0"}
            )
            print(f"Downloading {info['url']}")
            with urllib.request.urlopen(request, timeout=60) as response, target.open("wb") as output:
                shutil.copyfileobj(response, output)
        paths[key] = target
    return paths


def find_font(candidates: tuple[str, ...]) -> Path:
    for candidate in candidates:
        path = Path(candidate)
        if path.is_file():
            return path
    raise FileNotFoundError(f"No usable font found in: {candidates}")


def filter_path(path: Path) -> str:
    return path.resolve().as_posix().replace(":", r"\:")


def filter_text(text: str) -> str:
    return text.replace("\\", r"\\").replace(":", r"\:").replace("'", r"\'")


def generate_music(path: Path, duration: float = TOTAL_DURATION, sample_rate: int = 48000) -> None:
    """Generate an original warm electronic bed without external samples."""
    rng = np.random.default_rng(3800)
    frame_count = int(round(duration * sample_rate))
    left = np.zeros(frame_count, dtype=np.float32)
    right = np.zeros(frame_count, dtype=np.float32)
    bpm = 105.0
    beat = 60.0 / bpm
    bar = beat * 4.0
    progression = (
        (130.81, 155.56, 196.00),
        (103.83, 130.81, 155.56),
        (155.56, 196.00, 233.08),
        (116.54, 146.83, 174.61),
    )

    def add(signal: np.ndarray, start: float, pan: float = 0.0) -> None:
        start_index = max(0, int(round(start * sample_rate)))
        end_index = min(frame_count, start_index + len(signal))
        if end_index <= start_index:
            return
        signal = signal[: end_index - start_index]
        left_gain = math.sqrt((1.0 - pan) * 0.5)
        right_gain = math.sqrt((1.0 + pan) * 0.5)
        left[start_index:end_index] += signal * left_gain
        right[start_index:end_index] += signal * right_gain

    bar_index = 0
    start = 0.0
    while start < duration:
        chord = progression[bar_index % len(progression)]
        pad_duration = min(bar + 0.25, duration - start)
        pad_t = np.arange(int(pad_duration * sample_rate), dtype=np.float32) / sample_rate
        pad_env = np.minimum(pad_t / 0.35, 1.0) * np.minimum((pad_duration - pad_t) / 0.45, 1.0)
        pad_env = np.clip(pad_env, 0.0, 1.0)
        pad = np.zeros_like(pad_t)
        for harmonic, frequency in enumerate(chord, start=1):
            pad += np.sin(2.0 * np.pi * frequency * pad_t + harmonic * 0.37) * (0.016 / harmonic)
            pad += np.sin(2.0 * np.pi * frequency * 2.0 * pad_t) * (0.0035 / harmonic)
        add((pad * pad_env).astype(np.float32), start, pan=-0.22 if bar_index % 2 == 0 else 0.22)

        for beat_index in range(4):
            event_time = start + beat_index * beat
            if event_time >= duration:
                continue

            bass_frequency = chord[0] / 2.0
            bass_duration = 0.45
            bass_t = np.arange(int(bass_duration * sample_rate), dtype=np.float32) / sample_rate
            bass_env = np.exp(-5.2 * bass_t)
            bass = (np.sin(2.0 * np.pi * bass_frequency * bass_t) + 0.2 * np.sin(4.0 * np.pi * bass_frequency * bass_t))
            add((0.095 * bass * bass_env).astype(np.float32), event_time)

            kick_duration = 0.24
            kick_t = np.arange(int(kick_duration * sample_rate), dtype=np.float32) / sample_rate
            kick_phase = 2.0 * np.pi * (46.0 * kick_t + 8.0 * (1.0 - np.exp(-18.0 * kick_t)))
            kick = 0.33 * np.sin(kick_phase) * np.exp(-14.0 * kick_t)
            add(kick.astype(np.float32), event_time)

            if beat_index in (1, 3):
                snare_duration = 0.20
                snare_t = np.arange(int(snare_duration * sample_rate), dtype=np.float32) / sample_rate
                noise = rng.normal(0.0, 1.0, len(snare_t)).astype(np.float32)
                snare = 0.065 * noise * np.exp(-18.0 * snare_t)
                snare += 0.025 * np.sin(2.0 * np.pi * 190.0 * snare_t) * np.exp(-17.0 * snare_t)
                add(snare.astype(np.float32), event_time, pan=0.10)

            for half in (0.0, beat * 0.5):
                hat_time = event_time + half
                hat_duration = 0.055
                hat_t = np.arange(int(hat_duration * sample_rate), dtype=np.float32) / sample_rate
                noise = rng.normal(0.0, 1.0, len(hat_t)).astype(np.float32)
                hat = 0.025 * np.diff(noise, prepend=noise[0]) * np.exp(-55.0 * hat_t)
                add(hat.astype(np.float32), hat_time, pan=-0.35 if half == 0.0 else 0.35)

            note = chord[(beat_index + bar_index) % len(chord)] * 2.0
            pluck_duration = 0.38
            pluck_t = np.arange(int(pluck_duration * sample_rate), dtype=np.float32) / sample_rate
            pluck = (
                np.sin(2.0 * np.pi * note * pluck_t)
                + 0.35 * np.sin(4.0 * np.pi * note * pluck_t)
                + 0.12 * np.sin(6.0 * np.pi * note * pluck_t)
            )
            pluck *= 0.045 * np.exp(-8.0 * pluck_t)
            add(pluck.astype(np.float32), event_time + beat * 0.5, pan=-0.45 + 0.30 * beat_index)

        bar_index += 1
        start += bar

    fade_frames = int(1.5 * sample_rate)
    fade = np.linspace(0.0, 1.0, fade_frames, dtype=np.float32)
    left[:fade_frames] *= fade
    right[:fade_frames] *= fade
    left[-fade_frames:] *= fade[::-1]
    right[-fade_frames:] *= fade[::-1]
    peak = max(float(np.max(np.abs(left))), float(np.max(np.abs(right))), 1e-6)
    gain = 0.72 / peak
    stereo = np.column_stack((left * gain, right * gain))
    pcm = np.clip(stereo * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(pcm.tobytes())


def render_still_scene(
    scene: StillScene,
    asset_paths: dict[str, Path],
    output: Path,
    regular_font: Path,
    bold_font: Path,
) -> None:
    light = scene.theme == "light"
    c0, c1 = ("0xFFF8E9", "0xDCEBEC") if light else ("0x0D1720", "0x244550")
    ink = "0x101820" if light else "white"
    muted = "0x43525E" if light else "0xC8D2D8"
    accent = "0xD88428" if light else "0xF6C879"
    regular = filter_path(regular_font)
    bold = filter_path(bold_font)
    title = filter_text(scene.title)
    subtitle = filter_text(scene.subtitle)
    art_filters = []
    if scene.rotate_180:
        art_filters.extend(("hflip", "vflip"))
    art_filters.extend((
        "scale=850:820:force_original_aspect_ratio=decrease",
        "format=rgba",
        "fade=t=in:st=0:d=0.45:alpha=1",
        f"fade=t=out:st={scene.duration - 0.45:.2f}:d=0.45:alpha=1",
    ))
    filters = [
        "[0:v]drawgrid=width=96:height=96:thickness=1:color=0xFFFFFF@0.035[grid]",
        "[grid]drawbox=x=74:y=66:w=1772:h=948:color=0xFFFFFF@0.035:t=fill[base]",
        f"[1:v]{','.join(art_filters)}[art]",
    ]
    base_label = "base"
    if scene.card:
        filters.append(
            "[base]drawbox=x=1010:y=105:w=810:h=870:color=0xFFFFFF@0.97:t=fill[card]"
        )
        base_label = "card"
    filters.append(
        f"[{base_label}][art]overlay=x='1050+12*sin(t*0.55)':y='130+8*cos(t*0.45)':format=auto[arted]"
    )
    current = "arted"
    if scene.logo:
        filters.extend(
            (
                "[2:v]scale=290:-1:force_original_aspect_ratio=decrease,format=rgba[logo]",
                f"[{current}][logo]overlay=x=112:y=82:format=auto[logoed]",
            )
        )
        current = "logoed"
        header_y = 190
    else:
        filters.append(
            f"[{current}]drawtext=fontfile='{bold}':text='RASPIAUDIO  /  VOICE DSP+':"
            f"x=112:y=83:fontsize=25:fontcolor={accent}[branded]"
        )
        current = "branded"
        header_y = 165

    title_size = 50 if len(scene.title) > 28 else 58
    filters.append(
        f"[{current}]drawbox=x=112:y={header_y - 10}:w=9:h=184:color={accent}:t=fill,"
        f"drawtext=fontfile='{bold}':text='{title}':x=151:y={header_y}:fontsize={title_size}:fontcolor={ink},"
        f"drawtext=fontfile='{regular}':text='{subtitle}':x=151:y={header_y + 82}:fontsize=28:fontcolor={muted}[copy]"
    )
    current = "copy"
    bullet_y = 520
    for index, bullet in enumerate(scene.bullets):
        y = bullet_y + index * 70
        next_label = f"bullet{index}"
        filters.append(
            f"[{current}]drawbox=x=151:y={y + 7}:w=16:h=16:color={accent}:t=fill,"
            f"drawtext=fontfile='{regular}':text='{filter_text(bullet)}':x=190:y={y}:"
            f"fontsize=28:fontcolor={ink}[{next_label}]"
        )
        current = next_label
    filters.append(
        f"[{current}]fade=t=in:st=0:d=0.30,fade=t=out:st={scene.duration - 0.30:.2f}:d=0.30,"
        "format=yuv420p[vout]"
    )

    command = [
        "ffmpeg",
        "-y",
        "-hide_banner",
        "-loglevel",
        "warning",
        "-f",
        "lavfi",
        "-i",
        f"gradients=s={WIDTH}x{HEIGHT}:r={FPS}:d={scene.duration}:c0={c0}:c1={c1}:x0=0:y0=0:x1={WIDTH}:y1={HEIGHT}:speed=0.002",
        "-loop",
        "1",
        "-framerate",
        str(FPS),
        "-i",
        str(asset_paths[scene.asset]),
    ]
    if scene.logo:
        command.extend(("-loop", "1", "-framerate", str(FPS), "-i", str(asset_paths["logo"])))
    command.extend(
        (
            "-filter_complex",
            ";".join(filters),
            "-map",
            "[vout]",
            "-an",
            "-t",
            f"{scene.duration:.3f}",
            "-r",
            str(FPS),
            "-c:v",
            "libx264",
            "-preset",
            "medium",
            "-crf",
            "20",
            "-pix_fmt",
            "yuv420p",
            str(output),
        )
    )
    run(command)


def render_demo_scene(key: str, duration: float, source: Path, seek: float, output: Path) -> None:
    labels = {
        "08-aec": "REAL AEC TEST  /  SPEAKER BEAT + VOICE  /  OFF TO ON",
        "09-noise": "REAL NOISE SUPPRESSION TEST  /  BEFORE TO AFTER",
        "10-wake": "HEY JARVIS  /  MODEL RUNS ON RASPBERRY PI",
    }
    filters = (
        f"scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease,"
        f"pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2:color=0x0D1720,"
        "drawbox=x=0:y=0:w=1920:h=52:color=0x0D1720@0.92:t=fill,"
        "drawtext=fontfile='C\\:/Windows/Fonts/segoeuib.ttf':"
        f"text='{labels[key]}':x=54:y=13:fontsize=24:fontcolor=0xF6C879,"
        f"fade=t=in:st=0:d=0.30,fade=t=out:st={duration - 0.30:.2f}:d=0.30,format=yuv420p"
    )
    run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "warning",
            "-ss",
            f"{seek:.3f}",
            "-i",
            str(source),
            "-t",
            f"{duration:.3f}",
            "-vf",
            filters,
            "-an",
            "-r",
            str(FPS),
            "-c:v",
            "libx264",
            "-preset",
            "medium",
            "-crf",
            "20",
            "-pix_fmt",
            "yuv420p",
            str(output),
        ]
    )


def render_close(asset_paths: dict[str, Path], output: Path, regular_font: Path, bold_font: Path) -> None:
    duration = 3.0
    regular = filter_path(regular_font)
    bold = filter_path(bold_font)
    filters = ";".join(
        (
            "[0:v]drawgrid=width=96:height=96:thickness=1:color=0xFFFFFF@0.035[grid]",
            "[1:v]hflip,vflip,scale=480:620:force_original_aspect_ratio=decrease,format=rgba,"
            "fade=t=in:st=0:d=0.35:alpha=1[product]",
            "[2:v]scale=330:-1:force_original_aspect_ratio=decrease,format=rgba[logo]",
            "[grid][product]overlay=x=1210:y=245:format=auto[p]",
            "[p]drawbox=x=110:y=122:w=840:h=720:color=0xFFFFFF@0.92:t=fill[card]",
            "[card][logo]overlay=x=180:y=200:format=auto[branded]",
            f"[branded]drawtext=fontfile='{bold}':text='VOICE DSP+':x=180:y=330:fontsize=80:fontcolor=0x101820,"
            f"drawtext=fontfile='{regular}':text='USB when you want simple.':x=180:y=470:fontsize=34:fontcolor=0x34414E,"
            f"drawtext=fontfile='{regular}':text='Raspberry Pi when you want control.':x=180:y=525:fontsize=34:fontcolor=0x34414E,"
            f"drawtext=fontfile='{bold}':text='raspiaudio.com/product/voicedsp/':x=180:y=665:fontsize=38:fontcolor=0xD88428,"
            f"fade=t=in:st=0:d=0.30,fade=t=out:st={duration - 0.30:.2f}:d=0.30,format=yuv420p[vout]",
        )
    )
    run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "warning",
            "-f",
            "lavfi",
            "-i",
            f"gradients=s={WIDTH}x{HEIGHT}:r={FPS}:d={duration}:c0=0xF6C879:c1=0x89B4BA:x0=0:y0=0:x1={WIDTH}:y1={HEIGHT}:speed=0.004",
            "-loop",
            "1",
            "-framerate",
            str(FPS),
            "-i",
            str(asset_paths["pcb"]),
            "-loop",
            "1",
            "-framerate",
            str(FPS),
            "-i",
            str(asset_paths["logo"]),
            "-filter_complex",
            filters,
            "-map",
            "[vout]",
            "-an",
            "-t",
            f"{duration:.3f}",
            "-r",
            str(FPS),
            "-c:v",
            "libx264",
            "-preset",
            "medium",
            "-crf",
            "20",
            "-pix_fmt",
            "yuv420p",
            str(output),
        ]
    )


def probe(path: Path) -> dict[str, object]:
    result = subprocess.run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format=duration,size,bit_rate:stream=index,codec_name,codec_type,width,height,r_frame_rate,sample_rate,channels",
            "-of",
            "json",
            str(path),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(result.stdout)


def build(output: Path, poster: Path, manifest: Path, refresh: bool) -> None:
    for tool in ("ffmpeg", "ffprobe"):
        if shutil.which(tool) is None:
            raise FileNotFoundError(f"{tool} is required on PATH")
    regular_font = find_font(
        (r"C:\Windows\Fonts\bahnschrift.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")
    )
    bold_font = find_font(
        (r"C:\Windows\Fonts\segoeuib.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")
    )
    asset_paths = download_assets(refresh)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    segment_paths: list[Path] = []

    for scene in STILL_SCENES:
        segment = BUILD_DIR / f"{scene.key}.mp4"
        render_still_scene(scene, asset_paths, segment, regular_font, bold_font)
        segment_paths.append(segment)
    for key, duration, asset_key, seek in DEMO_SCENES:
        segment = BUILD_DIR / f"{key}.mp4"
        render_demo_scene(key, duration, asset_paths[asset_key], seek, segment)
        segment_paths.append(segment)
    close_segment = BUILD_DIR / "11-close.mp4"
    render_close(asset_paths, close_segment, regular_font, bold_font)
    segment_paths.append(close_segment)

    concat_list = BUILD_DIR / "segments.txt"
    concat_list.write_text(
        "".join(f"file '{path.resolve().as_posix()}'\n" for path in segment_paths), encoding="utf-8"
    )
    silent_video = BUILD_DIR / "voice-dsp-plus-overview-silent.mp4"
    run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "warning",
            "-f",
            "concat",
            "-safe",
            "0",
            "-i",
            str(concat_list),
            "-c",
            "copy",
            str(silent_video),
        ]
    )

    music = BUILD_DIR / "original-tech-bed.wav"
    generate_music(music)
    output.parent.mkdir(parents=True, exist_ok=True)
    mix_filter = ";".join(
        (
            "[1:a]atrim=start=0:end=90,asetpts=PTS-STARTPTS,"
            "volume='if(lt(t,65.5),0.23,if(lt(t,66),0.23-0.38*(t-65.5),"
            "if(lt(t,87),0.04,if(lt(t,87.5),0.04+0.38*(t-87),0.23))))':eval=frame[music]",
            "[2:a]atrim=start=2.5:end=9.5,asetpts=PTS-STARTPTS,aresample=48000,"
            "volume=1.15,afade=t=in:st=0:d=0.12,afade=t=out:st=6.82:d=0.18,"
            "adelay=66000:all=1[aec]",
            "[3:a]atrim=start=4:end=11,asetpts=PTS-STARTPTS,aresample=48000,"
            "volume=1.10,afade=t=in:st=0:d=0.12,afade=t=out:st=6.82:d=0.18,"
            "adelay=73000:all=1[noise]",
            "[4:a]atrim=start=8:end=15,asetpts=PTS-STARTPTS,aresample=48000,"
            "volume=1.20,afade=t=in:st=0:d=0.12,afade=t=out:st=6.82:d=0.18,"
            "adelay=80000:all=1[wake]",
            "[music][aec][noise][wake]amix=inputs=4:duration=longest:normalize=0,"
            "alimiter=limit=0.92:level=0,loudnorm=I=-16:TP=-2.2:LRA=9,"
            "alimiter=limit=0.75:level=0[aout]",
        )
    )
    run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "warning",
            "-i",
            str(silent_video),
            "-i",
            str(music),
            "-i",
            str(asset_paths["aec_demo"]),
            "-i",
            str(asset_paths["noise_demo"]),
            "-i",
            str(asset_paths["wake_demo"]),
            "-filter_complex",
            mix_filter,
            "-map",
            "0:v:0",
            "-map",
            "[aout]",
            "-c:v",
            "copy",
            "-c:a",
            "aac",
            "-b:a",
            "192k",
            "-ar",
            "48000",
            "-t",
            f"{TOTAL_DURATION:.3f}",
            "-movflags",
            "+faststart",
            str(output),
        ]
    )
    run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "warning",
            "-ss",
            "4.0",
            "-i",
            str(output),
            "-frames:v",
            "1",
            "-update",
            "1",
            "-q:v",
            "2",
            str(poster),
        ]
    )

    asset_records = []
    for key, info in ASSETS.items():
        path = asset_paths[key]
        asset_records.append(
            {
                "key": key,
                "kind": info["kind"],
                "url": info["url"],
                "filename": info["filename"],
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
        )
    payload = {
        "schema": 1,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "title": "RASPIAUDIO Voice DSP+ product overview",
        "timeline_seconds": TOTAL_DURATION,
        "storyboard": [
            {"start": 0, "end": 7, "content": "Product promise"},
            {"start": 7, "end": 18, "content": "USB-C plug-and-play and Web Updater"},
            {"start": 18, "end": 29, "content": "Microphones and audio outputs"},
            {"start": 29, "end": 39, "content": "On-board voice processing"},
            {"start": 39, "end": 48, "content": "LINE and SQUARE geometries"},
            {"start": 48, "end": 58, "content": "Advanced Raspberry Pi mode"},
            {"start": 58, "end": 66, "content": "Applications"},
            {"start": 66, "end": 73, "content": "Real AEC demo audio"},
            {"start": 73, "end": 80, "content": "Real noise suppression demo audio"},
            {"start": 80, "end": 87, "content": "Far-field demo; model on Raspberry Pi"},
            {"start": 87, "end": 90, "content": "Product URL"},
        ],
        "music": {
            "origin": "Original algorithmic composition generated by build_voice_dsp_plus_promo.py",
            "third_party_samples": False,
            "voice_over": False,
            "ducked_during_real_demos_seconds": [66, 87],
        },
        "real_demo_audio": [
            {"asset": "aec_demo", "source_seconds": [2.5, 9.5], "timeline_seconds": [66, 73]},
            {"asset": "noise_demo", "source_seconds": [4, 11], "timeline_seconds": [73, 80]},
            {"asset": "wake_demo", "source_seconds": [8, 15], "timeline_seconds": [80, 87]},
        ],
        "assets": asset_records,
        "output": {
            "filename": output.name,
            "bytes": output.stat().st_size,
            "sha256": sha256(output),
            "probe": probe(output),
        },
        "poster": {
            "filename": poster.name,
            "bytes": poster.stat().st_size,
            "sha256": sha256(poster),
        },
    }
    manifest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"VIDEO={output}")
    print(f"POSTER={poster}")
    print(f"MANIFEST={manifest}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build the 90-second RASPIAUDIO Voice DSP+ product video.")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--poster", type=Path, default=DEFAULT_POSTER)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--refresh", action="store_true", help="Download every upstream asset again.")
    args = parser.parse_args()
    build(args.output.resolve(), args.poster.resolve(), args.manifest.resolve(), args.refresh)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

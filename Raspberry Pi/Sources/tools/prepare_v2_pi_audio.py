"""Generate review-only Pi ALSA/overlay files. No install, PCM open or boot changes."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def render_alsa(card: str, playback_device: int, capture_device: int, rate: int) -> str:
    if not re.fullmatch(r"[A-Za-z0-9_]+", card):
        raise ValueError("Use the explicit ALSA card ID, not a path/config expression")
    if rate not in (16000, 48000) or not all(0 <= n <= 31 for n in (playback_device, capture_device)):
        raise ValueError("Rate must be 16000/48000, device numbers 0..31")
    key = 0x56530000 + rate
    return f'''# V2 paired .6 I2S only; generated review file, NOT installed.
# ALSA_CONFIG_PATH can select this on a future test Pi; do not replace defaults.
</usr/share/alsa/alsa.conf>
pcm.voicedsp_v2_dmix {{
    type dmix
    ipc_key {key}
    ipc_key_add_uid true
    ipc_perm 0600
    slave {{
        pcm "hw:{card},{playback_device}"
        format S32_LE
        rate {rate}
        channels 2
        period_time 0
        period_size 256
        buffer_size 1024
    }}
    bindings {{ 0 0 1 1 }}
}}
pcm.voicedsp_v2_dsnoop {{
    type dsnoop
    ipc_key {key + 1}
    ipc_key_add_uid true
    ipc_perm 0600
    slave {{
        pcm "hw:{card},{capture_device}"
        format S32_LE
        rate {rate}
        channels 2
        period_time 0
        period_size 256
        buffer_size 1024
    }}
    bindings {{ 0 0 1 1 }}
}}
pcm.voicedsp_v2_playback {{ type plug slave.pcm "voicedsp_v2_dmix" }}
pcm.voicedsp_v2_capture {{ type plug slave.pcm "voicedsp_v2_dsnoop" }}
# Select processed channel 0 explicitly, never average it with channel 1.
pcm.voicedsp_v2_processed {{
    type route
    slave {{ pcm "voicedsp_v2_dsnoop" channels 2 }}
    ttable.0.0 1
}}
pcm.voicedsp_v2 {{
    type asym
    playback.pcm "voicedsp_v2_playback"
    capture.pcm "voicedsp_v2_capture"
}}
'''


def prepare(out: Path, card: str, playback: int, capture: int, rate: int, legacy: bool = False):
    config = render_alsa(card, playback, capture, rate)
    template = ROOT / "host/v2/pi/voicedsp-v2-overlay.dts"
    overlay = template.read_text()
    if legacy:
        overlay = overlay.replace("<&i2s_clk_producer>", "<&i2s>")
    out.mkdir(parents=True, exist_ok=False)
    files = {"voicedsp-v2.conf": config, "voicedsp-v2-overlay.dts": overlay}
    for name, text in files.items():
        (out / name).write_bytes(text.encode())
    report = {"hardware_tested": False, "installed": False,
              "card": card, "playback_device": playback, "capture_device": capture,
              "rate": rate, "i2s_controller": "i2s" if legacy else "i2s_clk_producer",
              "inputs": {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (template, Path(__file__))},
              "outputs": {name: hashlib.sha256(text.encode()).hexdigest() for name, text in files.items()}}
    (out / "MANIFEST.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--card", required=True)
    p.add_argument("--playback-device", type=int, required=True)
    p.add_argument("--capture-device", type=int, required=True)
    p.add_argument("--rate", type=int, choices=(16000, 48000), required=True)
    p.add_argument("--legacy-i2s-symbol", action="store_true")
    a = p.parse_args()
    prepare(a.output.resolve(), a.card, a.playback_device, a.capture_device, a.rate, a.legacy_i2s_symbol)
    print(f"Generated review files in {a.output.resolve()}; no system configuration changed")

#!/usr/bin/env python3
"""Set repeatable and safe Conexant levels for the comparison test."""

from __future__ import annotations

import argparse

from pycaw.pycaw import AudioUtilities


SPEAKER_NAME = "Speakers / Headphones (Conexant SmartAudio HD)"
MICROPHONE_NAME = "Internal Microphone (Conexant SmartAudio HD)"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--speaker-level", type=float, default=0.35)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 0.0 <= args.speaker_level <= 1.0:
        raise SystemExit("--speaker-level must be between 0.0 and 1.0")

    found: set[str] = set()
    for device in AudioUtilities.GetAllDevices():
        name = device.FriendlyName
        if name not in (SPEAKER_NAME, MICROPHONE_NAME):
            continue
        if not str(device.state).endswith("Active"):
            continue

        volume = device.EndpointVolume
        volume.SetMute(0, None)
        if name == SPEAKER_NAME:
            volume.SetMasterVolumeLevelScalar(args.speaker_level, None)
        else:
            volume.SetMasterVolumeLevelScalar(1.0, None)

        print(
            f"{name}: scalar={volume.GetMasterVolumeLevelScalar():.3f}, "
            f"dB={volume.GetMasterVolumeLevel():.2f}, mute={volume.GetMute()}"
        )
        found.add(name)

    missing = {SPEAKER_NAME, MICROPHONE_NAME} - found
    if missing:
        raise SystemExit(f"Missing active endpoint(s): {sorted(missing)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

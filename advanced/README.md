# Advanced Firmware

Most users should use the two firmware choices documented in the main README.
This directory keeps specialist images out of the normal setup path.

| Firmware | Purpose |
| --- | --- |
| `usb-16k-auto` | Voice-optimized 16 kHz USB mode with automatic geometry |
| `usb-6ch-16k-line` | Diagnostic capture: four raw microphones plus two extra channels, forced LINE |
| `usb-6ch-16k-square` | Diagnostic capture: four raw microphones plus two extra channels, forced SQUARE |
| `raspberry-pi-48k-auto` | 48 kHz Raspberry Pi I2S mode with automatic geometry |

The 6-channel images are microphone-debug tools, not normal product firmware.
They are intended to confirm that all four physical microphones are active.
They do not replace the two-channel processed USB firmware.

## Noise-suppression tuning

The validated Raspberry Pi runtime settings, baseline procedure and comparison
tools are documented in [noise-suppression-demo.md](noise-suppression-demo.md).

## AEC tuning

The controlled AEC profiles and repeatable bass-heavy playback test are
documented in [aec-comparison.md](aec-comparison.md).

The selected double-talk candidate, corrected clean AEC OFF baseline and the
reason the test-only pipeline bypass was rejected are recorded in
[aec-davos-validation-2026-07-23.md](aec-davos-validation-2026-07-23.md).

The repeat measurement, listening extracts and every applied XVF3800 parameter
are archived in [aec-proof/README.md](aec-proof/README.md). Future firmware
integration work is tracked separately in
[aec-firmware-todo.md](aec-firmware-todo.md).

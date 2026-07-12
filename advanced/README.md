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

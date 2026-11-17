# Home Assistant r8: accepted 2026-11-17

Version: `0.6.75-qspi-lab-r8-ble.20261117`.

- BLE Improv setup for an ESP32 with no saved Wi-Fi, bounded to ten minutes;
  USB Improv and the fallback access point remain available.
- Fixed package merging so BLE no longer replaces the existing boot sequence.
- Restored idle DOA and timer feedback.
- Restored the actual saved output at boot, not merely the product preset default.
- Accepted idle-time settings are committed to nonvolatile memory.
- Preserved validated AEC, crossover, gains, wake models and XMOS payloads.

The owner confirmed DOA, output memorization and the HA timer on r8.
29 software tests and 24 native route/mode/stereo combinations pass. BLE
advertising was observed; a full BLE commissioning matrix and new acoustic AEC
qualification are not claimed.

**Known limitation:** one warm OTA restart left the audio controller unavailable;
a complete ten-second power-off recovered the board. If this occurs, stop audio
and remove every power source before reconnecting.

**First installation:** r8 resumes already provisioned XMOS I2S in QSPI. Blank or
USB-only boards need [Safe Recovery (I2S)](https://apps.raspiaudio.com/#device=ZeroCoreS3&application=USBRecovery)
first, then reinstall this Home Assistant image on the ZeroCore.

The stable/beta update feeds offer the exact accepted factory/OTA binaries.
The existing private production-programmer kit is not changed by this publication.
[Source](../Sources/ESPHome-BLE-r8) | [Downloads](README.md) | [Checksums](SHA256SUMS)

Factory SHA256: `35e4c78c6d35cc2b5045745ba556873f8ac0ddb72c61b1c4b00be93517bbee26`.
OTA SHA256: `b0261f965f9489ac1a74441ba69cbb34f1712c97e174a57985b36d4493231552`.

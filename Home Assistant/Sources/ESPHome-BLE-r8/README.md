# Accepted Home Assistant r8 source

Build entry point: `zerocore-v2-web.yaml`, with ESPHome 2026.8.1 / ESP-IDF 5.5.5.
Exact binary version: `0.6.75-qspi-lab-r8-ble.20261117`.

[Build instructions](../README.md) | [Release notes](../../Firmware/RELEASE-r8.md)

Includes the BLE boot-merge fix, output restore and NVS commit changes. Audio
processing, filters and XMOS payloads are preserved. No personal Wi-Fi credentials,
private validation logs or recordings are included. Dependency and asset licenses
remain in their directories and the shared source notices.

The original `ESPHome/` directory remains the pre-BLE 0.6.75 source for rollback.

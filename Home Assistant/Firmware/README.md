# Home Assistant release 0.6.75 r8

For **RASPIAUDIO ZeroCore S3 (16 MB flash / 8 MB octal PSRAM) + Voice DSP+ V2**.
Not V1 and not a FutureProof Satellite1 hardware image.

| Download | Use |
| --- | --- |
| [Factory image](voicedsp-v2-0.6.75-r8-ble.factory.bin) | ESP32 USB install, merged image at offset 0 |
| [OTA image](voicedsp-v2-0.6.75-r8-ble.ota.bin) | Existing compatible ESPHome installation; not an offset-0 image |
| [XMOS SPI image](xmos-i2s48.spi_boot.bin) | Already embedded in both ESP32 images |
| [Installer manifest](manifest.json) | ESP Web Tools integrators; this exact factory image |
| [Checksums](SHA256SUMS) | Verify downloaded bytes |

Normally use [the five-step guide](../README.md). For command-line USB install,
stop audio and identify the correct ESP32 port:

```sh
python -m pip install esptool==4.11.0
python -m esptool --chip esp32s3 --port YOUR_PORT write_flash 0x0 voicedsp-v2-0.6.75-r8-ble.factory.bin
```

Optional `erase_flash` removes Wi-Fi/preferences; use only for an intentional
clean install, not a routine update. Press RESET if it stays in download mode.

For compatible network OTA, from `Home Assistant/Sources/ESPHome-BLE-r8` with the pinned
ESPHome environment, stop music and supply the actual device address:

```sh
python -m esphome upload zerocore-v2-web.yaml --device DEVICE_IP --file ../../Firmware/voicedsp-v2-0.6.75-r8-ble.ota.bin
```

Allow at least 65 seconds for boot/mode restoration. Check controller-ready,
output and volume. Internal and external general gains are 0 dB and hidden by
default; use the main volume and start low. Filter trims are not general gain.
Existing preferences migrate; modes and filter changes apply when audio is idle.

Embedded XMOS SHA256:
`207b73b9ead1e84769ca4b8b208917ab2c5086fcd809b16edca11e224759c2bc`.
Routine boots resume persistent XMOS I2S. Blank/USB-only boards need Safe Recovery
(I2S) first. Explicit Install XMOS in Flash and Reload embedded XMOS firmware
are separate persistent and temporary actions. See [r8 release notes](RELEASE-r8.md).

[Provenance](release.json) records the owner-accepted r8 checkpoint (DOA, output memory and timer):
compilation, source/native tests, OTA and post-boot settings preservation passed.
PAJ recovery and timer controls are retained. AEC, wake models, XMOS and crossover
tuning are unchanged; no new acoustic qualification is claimed. Earlier downloads
remain for rollback, but are not selected by the installer or HA update catalog.

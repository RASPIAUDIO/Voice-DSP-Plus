# Paired Home Assistant release 0.6.69

For **RASPIAUDIO ZeroCore S3 (16 MB flash / 8 MB octal PSRAM) + Voice DSP+ V2**.
Not V1 and not a FutureProof Satellite1 hardware image.

| Download | Use |
| --- | --- |
| [Factory image](voicedsp-v2-0.6.69.factory.bin) | ESP32 USB install, merged image at offset 0 |
| [OTA image](voicedsp-v2-0.6.69.ota.bin) | Existing compatible ESPHome installation; not an offset-0 image |
| [XMOS SPI image](xmos-i2s48.spi_boot.bin) | Already embedded in both ESP32 images |
| [Installer manifest](manifest.json) | ESP Web Tools integrators; this exact factory image |
| [Checksums](SHA256SUMS) | Verify downloaded bytes |

Normally use [the five-step guide](../README.md). For command-line USB install,
stop audio and identify the correct ESP32 port:

```sh
python -m pip install esptool==4.11.0
python -m esptool --chip esp32s3 --port YOUR_PORT write_flash 0x0 voicedsp-v2-0.6.69.factory.bin
```

Optional `erase_flash` removes Wi-Fi/preferences; use only for an intentional
clean install, not a routine update. Press RESET if it stays in download mode.

For compatible network OTA, from `Home Assistant/Sources/ESPHome` with the pinned
ESPHome environment, stop music and supply the actual device address:

```sh
python -m esphome upload zerocore-v2-web.yaml --device DEVICE_IP --file ../../Firmware/voicedsp-v2-0.6.69.ota.bin
```

Allow at least 65 seconds for boot/mode restoration. Check controller-ready,
output and volume. First install defaults to internal -6 dB; Full Speaker's
common level starts at -24 dB. Existing preferences migrate.

Embedded XMOS SHA256:
`207b73b9ead1e84769ca4b8b208917ab2c5086fcd809b16edca11e224759c2bc`.
It loads into RAM at each boot; XMOS QSPI is not written by this release.

[Provenance](release.json) records the 0.6.69 checkpoint: compilation, native
timer-gesture tests, OTA and post-boot settings preservation passed. Physical
gesture-to-alarm dismissal acceptance is pending. PAJ recovery from 0.6.68 is
retained; that version passed repeated gestures and twelve repositioned wake
trials (four quiet, eight with music). No new acoustic claim is made for 0.6.69.
AEC, wake models and XMOS are unchanged. Earlier downloads remain for rollback.

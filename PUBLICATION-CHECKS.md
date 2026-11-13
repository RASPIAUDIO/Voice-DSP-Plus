# USB and Home Assistant publication checks

Publication prepared on **2026-11-13**. This packages accepted releases; it does
not introduce a new firmware build or claim additional hardware qualification.

| Check | Result |
| --- | --- |
| USB checkpoint | 0.8.9-r4; original four images and SHA256 retained |
| Home Assistant checkpoint | 0.6.61; original factory/OTA and paired XMOS retained |
| ESP32 factory versus OTA | Application bytes match exactly at offset 0x10000 |
| Embedded XMOS | Exact SPI image present in OTA and in the published source package |
| XMOS source reconstruction | 87 USB and 54 HA modified/new source files match the accepted source trees |
| USB host control | 9 offline Python tests pass |
| USB native C tests | Five test programs pass: filters, settings journal, buttons/state, UAC volume/mute |
| Filter comparisons | 256 fixed preset/level pairs and 28,800 parametric comparisons pass |
| ESPHome configuration | Published entry point validates with ESPHome 2026.8.1, including from a staged-file export |
| Documentation | Local links and installer manifest checked |

`USB/Sources/tools/verify_release.py` rechecks both distributions without device
access. Source rebuild requirements are in each `Sources/README.md`; no new
ESP32/XMOS cross-compilation or acoustic session was performed for publication.

Only original tones and models are included, not private room/voice recordings.
The Raspberry Pi kit and the public `main` branch are not changed. The external
web-installer catalogs are separate deployments and are not promoted here.

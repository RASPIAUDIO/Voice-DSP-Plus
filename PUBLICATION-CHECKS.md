# USB and Home Assistant publication checks

## Home Assistant timer-gesture update: 2026-11-14

0.6.69 changes only the ESP32 gesture action and release version. All ten PAJ
event names dismiss a ringing timer before normal actions. Native execution of
the real YAML lambda tests cooldown, neutral/disabled states and usual actions.
Compilation, OTA and post-boot settings preservation passed. Physical gesture
dismissal acceptance remains pending. No new AEC/wake qualification is claimed.
Sources and factory/OTA images match the installed candidate; XMOS, USB and
Raspberry Pi images remain unchanged. Prior binaries are retained for rollback.

## Home Assistant update: 2026-11-14

0.6.68 replaces the HA download only; USB and Raspberry Pi remain unchanged.
The installed factory/OTA hashes match the downloads and Apps manifests.
Public ESPHome configuration validates; XMOS source reconstruction verifies
against the licensed SDK archive. No SDK library sources or private recordings
are included. Voice PE sound attribution and model licenses are retained.

Owner accepted repeated PAJ gestures: 16 events and four mute/unmute cycles over
two minutes, no I2C errors or unavailable reports. Native tests inject NACKs and
verify bounded recovery and stale-event rejection. Existing AEC/filter settings
are preserved. Alternate wake words and response Stop await acoustic testing;
no new long-duration reliability or AEC qualification is claimed.

## Earlier publication

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

The initial release included original tones/models; the current HA update adds
attributed Voice PE sounds, never private room/voice recordings.
The Raspberry Pi kit and the public `main` branch are not changed.

## Web installer deployment

The subsequent 2026-11-13 web publication promotes these same images to the USB
updater and RASPIAUDIO Apps. Browser links preselect V2. Existing V1 entries and
images remain separate. Home Assistant stable/beta manifests both point to the
exact 0.6.61 application; update installation remains manual.

WebUSB tests use a simulated device, not an actual firmware installation. A
production-site check does not constitute a new hardware flashing test.

- Live USB index, versioned JavaScript/catalog and 0.8.9-r4 image match the
  staged bytes. Both legacy V1 firmware downloads still match their original hashes.
- Apps installer, stable and beta manifests serve 0.6.61. Its downloaded factory
  and OTA images match the accepted SHA256 values in `Home Assistant/Firmware`.
- `node USB/WebInstaller/test.cjs` passes the actual app's simulated 3,686-page
  DFU flow, target/partition/hash guards, settings-preserving mute and operation lock.
- No connected board was flashed during deployment. Interactive browser/device
  selection was not exercised; browser automation was unavailable in this session.

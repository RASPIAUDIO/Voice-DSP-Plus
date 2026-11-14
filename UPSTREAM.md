# Source and release provenance

## USB 0.8.9-r4

Accepted checkpoint `v2-usb-0.8.9-volume-retention-20261112`, commit
`b31c4d76edf56e98c6045ae8b69af1c9f954312b`.
Only candidate r4 is published. Image sizes and SHA256 are in
[USB release.json](USB/Firmware/release.json).

## Home Assistant 0.6.61

Accepted checkpoint `v2-0.6.61-absolute-levels-20261111`, commit
`50b5aad6db99f71f6fd1def6b3efef9069f7918b`.
The ESP32 factory/OTA images and matching embedded XMOS SPI image are unchanged.
See [HA release.json](Home%20Assistant/Firmware/release.json).

| Dependency / files | Version or source | Notice |
| --- | --- | --- |
| ESPHome framework and adapted `components/i2s_audio`, `components/voice_dsp_i2s_microphone` | ESPHome 2026.8.1; FutureProofHomes Satellite1-ESPHome base `9a58961438568872f2e27ae4c2aec257ea05f97a` | GPL-3.0 runtime / MIT Python; [GPL text](Home%20Assistant/Sources/licenses/LICENSE.FutureProofHomes) |
| ESP-IDF and audio dependencies | YAML and `ESPHome/dependencies.lock` pin the accepted dependency versions | Respective upstream licenses retained by the build system |
| Wake word and VAD assets | Per-model revision and SHA256 in [provenance.json](Home%20Assistant/Sources/ESPHome/assets/provenance.json) | [Apache-2.0](Home%20Assistant/Sources/ESPHome/assets/LICENSE) |
| Active startup, wake, mute, jack and timer sounds | Home Assistant Voice PE; source revision and hashes in asset provenance | [CC BY 4.0 attribution](Home%20Assistant/Sources/ESPHome/assets/voice-pe/LICENSE.md) |
| Archived synthetic tones | RASPIAUDIO-generated, retained for history | [CC0 declaration](Home%20Assistant/Sources/ESPHome/assets/ui-sounds/README.md) |
| PAJ7620U2 initialization | DFRobot_PAJ7620U2 `57edc1d` | [MIT](USB/Sources/licenses/PAJ-LICENSE.txt) |

Home Assistant 0.6.68 includes selectable Hey Jarvis, Okay Nabu and Hey Mycroft,
plus Stop and VAD. See [additional model provenance](Home%20Assistant/Sources/ESPHome/assets/WAKE_MODELS.md)
and the retained model licenses. Only one main wake model is active at a time.

## XMOS source integration

Both modes target XVF3800 SDK **3.2.1**, archive
`xvf3800_source_external_241029_121941.tar`, SHA256
`f69ce06cf689dcb702f369f1ad763b4d5b34a5197480bd57757dada7579f9e4d`.
Each `Sources/XMOS/changes.json` identifies every modified/new file, its hashes,
line edits and build target. `SOURCE_SHA256.json` provides the reconstructed
source checksums. Unmodified SDK libraries are not duplicated here.

Our original integration code is MIT. Adaptations retain their upstream notices;
the [XMOS package license](USB/Firmware/LICENSE.xmos-package.rst) applies to vendor
components and firmware. No personal credentials or private test recordings
are published. Publication does not change the AEC, filters or hardware tuning.

[Publication checks](PUBLICATION-CHECKS.md) distinguish source/package checks
from the earlier physical validation of the release binaries.

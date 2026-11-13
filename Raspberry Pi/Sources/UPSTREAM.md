# Source provenance

RASPIAUDIO Pi integration r8, exported 2026-11-13. Original code is MIT licensed.
Linux controller, portable drivers and transport match the tested acoustic core.
Publishing introduces no new AEC preset, speaker filter or firmware change.

- PAJ initialization table: DFRobot/DFRobot_PAJ7620U2 revision `57edc1d`;
  [DFRobot MIT notice](licenses/PAJ-LICENSE.txt).
- `esphome::voicedsp_v2` is the historical namespace of our portable UI modules.
  No ESPHome runtime, FutureProof YAML/sounds or wake-word model is included.
- XMOS command identifiers control separately supplied device firmware. No SDK
  library or proprietary DSP source is mirrored.
- OS packages are obtained from the Pi's configured repositories; their own
  upstream licenses continue to apply.

Public packaging adds a firmware-optional preparer, anonymizes test usernames
and gives explicit missing-firmware diagnostics. Private recordings, development
archives, credentials and machine paths are excluded.

# TODO: embed the validated AEC profile in firmware

The 2026-07-24 AEC checkpoint is currently applied through XVF host-control
commands after boot. The profile must be promoted into firmware defaults only
after the following work is complete.

- Port the exact values from [aec-proof/README.md](aec-proof/README.md) into
  the Raspberry Pi 16 kHz AUTO firmware defaults.
- Evaluate the same profile separately on USB 48 kHz AUTO. Do not assume that
  16 kHz tuning transfers unchanged to the 48 kHz pipeline.
- Preserve automatic LINE/SQUARE geometry, jack routing, speaker mute and
  NeoPixel/DOA behavior.
- Confirm that the AEC far-end reference is active for both USB playback and
  Raspberry Pi I2S playback.
- Keep the click-free OFF diagnostic on mux category 3; never use
  `SHF_BYPASS=1` as the public no-AEC comparison.
- Validate LINE and SQUARE, speaker and headphone, USB and I2S, cold boot and
  power cycle.
- Run matched double-talk tests at several acoustic ratios and require
  `AEC_AECCONVERGED=1`, no clipping, no periodic dropout and no material
  near-end STT regression.
- Recheck startup playback so the first second is not lost while the speaker
  route unmutes.
- Archive the final source overlay, compiled images, hashes and acceptance
  captures next to each promoted firmware.

Until this matrix passes, the values remain a validated runtime candidate, not
a universal firmware default.

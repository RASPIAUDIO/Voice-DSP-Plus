# USB web updater deployment source

[Open V2 updater](https://raspiaudio.com/voicedsp/webflasher/?firmware=v2-usb-mono-48k).
This directory versions the production UI; firmware bytes live in `../Firmware`.

Deployment mapping:

- `index.html` and `styles.css` at `/voicedsp/webflasher/`.
- `app.js` deployed as `app-v2-089-r3.js`.
- `firmwares.json` deployed as `firmwares-v2-089.json`.
- `../Firmware/voicedsp-v2-usb-mono-0.8.9.dfu_upgrade.bin` under `firmware/`.
- Existing V1 `../public/...` paths remain untouched on the server.

Versioned JS/catalog names prevent cached older pages from seeing incompatible
new metadata. Deploy the image, catalog and JS before atomically replacing the
index; retain a server-side backup. No WordPress or product-page edits needed.

The UI verifies image size/SHA256, upgrade alternate 1, 256-byte pages, USB
identity and V1/V2 separation. Only one operation is permitted at a time.
V2 playback is muted without changing output, level or filter; users must stop
their audio applications before starting. Blank/I2S-only boards are not USB DFU
targets. Never replace the audio driver or flash a factory file through this UI.

Run `node test.cjs` for simulated checks, without a browser or connected hardware.

UI revision r2 fixes the XMOS vendor response framing: one transport-status byte
followed by 56 audio-state bytes, matching `../Sources/tools/usb_mono_control.py`.
The original UI incorrectly requested 56 bytes and interpreted the status as the
schema, stopping before any DFU write. Firmware images are unchanged. Regression
tests now include framed responses with a nonzero DataView offset and reject
truncated responses, transport errors and invalid schemas.

UI revision r3 distinguishes codec initialization from control-service health.
In `vdsp_usb_runtime.inc`, status byte 1 is `audio.initialized`, which can remain
zero before USB audio clocks run; it is not a DFU readiness flag. The updater
accepts this idle case only with valid I/O, no fault/error, inactive route 0,
mute set, the unchanged route/level/filter plus mute request latched at a new
epoch, and two status samples showing runtime progress. Initialized audio still
requires an applied mute. Faults and stalled runtime remain blocked; status
details are logged for diagnosis. This is not an unconditional recovery bypass.

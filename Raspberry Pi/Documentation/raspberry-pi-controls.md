[Voice DSP+ V2](../../README.md) / [Raspberry Pi](../README.md) / Controls

# Raspberry Pi controls and diagnostics

For the **48 kHz Pi kit, desktop r8**. Run commands on the Pi as the
normal user who installed the kit, not as root. These instructions do not flash
firmware. Commands marked as diagnostics can temporarily change the microphone
signal received by **all** applications.

## Daily use: no terminal needed

| Control | Use |
| --- | --- |
| Speaker icon | Left-click: playback volume/mute. Right-click: output device. |
| Microphone icon | Left-click: recording volume/mute. Right-click: input device. |
| Voice DSP+ Output tray icon or application | Select Internal, External or Full Speaker. |

The native device name can be **VoiceDSP V2 I2S**. The desktop microphone is
processed mono at 48 kHz, not four independent microphone channels.

**Internal** drives the small onboard speakers. **External** is unfiltered and
requires a full-range speaker or a passive crossover. **Full Speaker** uses the
validated filter for the tested assembly: L woofer, R tweeter, LR4 9500 Hz,
woofer 0 dB / tweeter -6 dB. Never select External for a directly wired tweeter.

The selector briefly mutes playback, switches through the controller, checks
AEC readback and then restores mute. It leaves the volume unchanged. If a check
fails, it leaves playback muted rather than claiming the switch succeeded.

**Power warning before External / Full Speaker:** connect a USB-C PD charger
directly to Voice DSP+ (65 W or higher recommended). It can power both the board
and the Pi through the header. Power off before rewiring; disconnect the Pi's
USB-C supply. Never power both USB-C inputs at once. The selector waits for your
confirmation; this is a wiring acknowledgement, not automatic charger detection.

Equivalent desktop commands:

```sh
python3 /opt/voicedsp-desktop/output_selector.py --status
python3 /opt/voicedsp-desktop/output_selector.py --select internal
# Alternative for the matched two-way assembly:
python3 /opt/voicedsp-desktop/output_selector.py --select full --confirm-pd
```

For a full-range/passively filtered external speaker only:

```sh
python3 /opt/voicedsp-desktop/output_selector.py --select external --confirm-pd --confirm-full-range
```

These are alternative choices, not a sequence to run on every speaker setup.
The hardware output gain and desktop slider are separate controls. Desktop
setup uses hardware 0 dB and initially 25% desktop volume; later desktop volume
is remembered. Core route/gain settings are not persistent in this Pi kit.

## Controller commands

Define this shortcut once in the current shell:

```sh
vdsp() {
  python3 /opt/voicedsp-v2-48k/scripts/control.py \
    --socket /run/voicedsp-v2-48k/control.sock "$@"
}
vdsp status
```

All requests go through the running service, the **sole I2C owner**. Do not run
`i2cset`, bus scans or another direct-I2C host-control tool alongside it.
For ordinary desktop output switching, prefer the selector above.
Low-level `vdsp output` commands do not show the desktop confirmation: the same
PD-only external-output wiring requirement still applies.

| Command | Effect |
| --- | --- |
| `vdsp status` | Read-only JSON state and AEC parameter words. |
| `vdsp output off` | Disable the audio output. |
| `vdsp output internal` | Internal speakers. |
| `vdsp output external` | External amplifier, no two-way filter. |
| `vdsp output full` | External amplifier with verified Full Speaker filter. |
| `vdsp output line` | Line-output route. |
| `vdsp volume -24` | Hardware level, integer dB from -60 to 0. Start quietly. |
| `vdsp mute on` / `vdsp mute off` | Speaker mute, independent of desktop mute. |
| `vdsp mic-mute on` / `vdsp mic-mute off` | Microphone mute. |
| `vdsp gestures on` / `vdsp gestures off` | Enable/disable gesture actions. |

Native ALSA playback bypasses the desktop slider. Hardware 0 dB is not a promise
of safe acoustic level or available power for every supply and speaker.

## AEC: inspect the accepted settings

This is a read-only check; it does not play sound or change settings:

```sh
python3 /opt/voicedsp-v2-48k/scripts/verify_installation.py
vdsp status > voicedsp-status.json
python3 -m json.tool /opt/voicedsp-v2-48k/accepted-aec.json
```

`passed: true` verifies configuration/readback, **not acoustic performance**.
Mic mute or a temporary raw-capture mux will intentionally make this check fail.
Do not use `--startup` after desktop setup: that option requires core OFF/-30 dB.

| Accepted Pi setting | Value |
| --- | --- |
| Profile, both internal and external | 4 |
| `PP_MIN_NS` / `PP_MIN_NN` | 0.10 / 0.51 |
| `PP_ATTNS_MODE` | 0 (disabled) |
| `AEC_FAR_EXTGAIN` | -24 |
| `PP_AGCONOFF` / `PP_AGCGAIN` | 0 / 5 |
| `PP_ECHOONOFF` / `PP_NLATTENONOFF` | 0 / 1 |
| `PP_DTSENSITIVE` | 10 |
| ASR output / system delay | 0 / -32 |
| Processed capture mux | `[8, 0, 8, 1]` |

Names are XMOS parameter identifiers, not simplified on/off descriptions of the
whole pipeline. In particular, `PP_ECHOONOFF=0` does **not** mean the complete
AEC is disabled. The accepted result includes the other AEC stages and reference.
The complete 24-word policy is in
[accepted-aec.json](../Sources/accepted-aec.json).
Some words encode IEEE-754 floats; do not read the decimal integer bit patterns
as physical gains. Convergence/path-change words are dynamic, not fixed presets.

To label the current raw readbacks without misinterpreting their type:

```sh
vdsp status > voicedsp-status.json
python3 - <<'PY'
import json
from pathlib import Path
s = json.loads(Path('voicedsp-status.json').read_text())
p = json.loads(Path('/opt/voicedsp-v2-48k/accepted-aec.json').read_text())
for i, (name, word) in enumerate(zip(p['parameter_names'], s['aec_parameter_words'])):
    valid = bool(s['aec_parameter_mask'] & (1 << i))
    print(f'{name:24s} word=0x{word:08x} valid={valid}')
PY
```

### Can I adjust the AEC?

There is currently **no arbitrary `set NS`, `set NN` or `set delay` command** in
the Pi controller. `accepted-aec.json` is a verification policy, not a live
configuration file: editing it does not tune the DSP.

The existing `aec-profile default|voice|legacy` commands are diagnostic policy
selectors, accepted only with the output OFF. `default` is the validated profile4
on both outputs; `voice` currently gives the same acoustic policy. `legacy`
selects the rejected older profile6 on Internal and is not recommended for use.
Do not copy USB/HA parameter sets into this Pi kit unverified.

To return from a diagnostic profile, stop playback first:

```sh
vdsp output off
vdsp aec-profile default
vdsp capture processed
# Then choose the appropriate output using Voice DSP+ Output.
# Allow a few seconds for readback, then run verify_installation.py again.
```

A custom AEC tune requires changing the controller's profile implementation,
rebuilding and comparing matched acoustic captures. Existing profile definitions:
[AecDiag](../Sources/drivers/v2_aec_diag.h).
Changing volume/filter/reference timing can change AEC results; repeat matched
acoustic comparisons rather than relying on a configuration readback alone.

## DOA: direction of arrival

DOA (sometimes called "DWA" in conversation) is the estimated sound direction.
The XMOS drives the LED direction display at idle; mute/UI animations can take
over the ring. The displayed direction is smoothed, not an instantaneous raw angle.

**Numeric/raw DOA angles are not exposed in the current Pi `status` response.**
There is no supported `vdsp doa` command. `geometry` describes the selected array,
not the arrival angle: 1 = internal SQUARE, 2 = external SQUARE, 3 = external LINE.

Reading angles continuously will need a controller extension with timestamp,
validity and a distinction between raw DSP angle and PCB-corrected display angle.
Do not bypass the controller with concurrent I2C reads. The XMOS display mapping
and smoothing are separate from this Linux host's status interface.

## Capture processed and raw audio

For everyday processed mono recording through the desktop:

```sh
arecord -r 48000 -c 1 -f S16_LE -d 8 my-voice.wav
```

For diagnostic captures, close microphone-using applications first. The mux is
global: while RAW is selected, the desktop mic also receives raw audio. A raw
capture bypasses processing on the **recorded tap**; it does not shut down the
DSP or alter the validated AEC coefficients.

| Command | I2S diagnostic output, two slots |
| --- | --- |
| `vdsp capture processed` | Original processed pair; accepted mux `[8,0,8,1]`. |
| `vdsp capture raw` | Raw microphone channels 0 and 1; `[3,0,3,1]`. |
| `vdsp capture reference` | Far-end reference0 in both slots; `[12,0,12,0]`. |
| `vdsp capture alignment` | Raw mic0 and reference0 together; `[3,0,12,0]`. |
| `vdsp capture residual` | Diagnostic residual pair; source7 channels0/1. |

Reference audio comes from playback **through Voice DSP+**, not from Realtek or
an unrelated room speaker. Four physical microphones do not give four simultaneous
raw I2S slots. This CLI currently exposes raw0/1, not a selection of raw2/3.
Use the dedicated four-mic diagnostic workflow for validating all four elements;
do not declare them healthy from this two-channel recording alone. Channel indices
are DSP signal indices, not a guarantee of the physical silkscreen microphone order.

### Short raw recording with automatic cleanup

After defining `vdsp` above, run this block. It waits for confirmed mux readback,
records eight seconds and explicitly restores processed capture even on failure:

```sh
(
  set -e
  trap 'vdsp capture processed >/dev/null' EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
  vdsp capture raw >/dev/null
  python3 - <<'PY'
import sys, time
sys.path.insert(0, '/opt/voicedsp-v2-48k/scripts')
from control import request
deadline = time.monotonic() + 10
while True:
    s = request('/run/voicedsp-v2-48k/control.sock', 'status')
    if s['ready'] and s['aec_mux_valid'] and s['aec_mux'] == [3, 0, 3, 1] and not s['aec_error']:
        break
    if time.monotonic() >= deadline:
        raise SystemExit('Raw mux not confirmed; recording cancelled')
    time.sleep(0.2)
PY
  ALSA_CONFIG_PATH=/etc/voicedsp-v2-48k/voicedsp-v2.conf \
    arecord -D voicedsp_v2_capture -r 48000 -c 2 -f S32_LE \
    --buffer-size=8192 --period-size=1024 -d 8 raw-mics-01.wav
)
python3 /opt/voicedsp-v2-48k/scripts/verify_installation.py
```

The controller also restores the original mux after **90 seconds** of a diagnostic
request. Keep captures shorter; a longer file could contain both raw and processed
segments. Do not replay raw captures loudly: unprocessed room/beat levels can be high.
Check the recording log for overruns, even when `arecord` exits successfully.

## Troubleshooting and evidence

```sh
systemctl status voicedsp-v2-48k.service --no-pager
journalctl -u voicedsp-v2-48k.service -b -n 100 --no-pager
systemctl --user status voicedsp-desktop.service --no-pager
journalctl --user -u voicedsp-desktop.service -b -n 100 --no-pager
```

`requested_route` can differ from `actual_route` while muted or blocked. A
`crossover_verified: false` with the output OFF does not alone establish a filter
failure; coefficients are checked before Full Speaker enables the amplifier.
Power Good is diagnostic in this kit, not a measured supply-voltage value.

Commands checked against the current controller and a read-only Pi status;
the accepted AEC verification passed without mismatches. Examples of diagnostic
capture do not constitute a new acoustic benchmark.

[Installation guide](../README.md) | [Release scope](release-notes.md)

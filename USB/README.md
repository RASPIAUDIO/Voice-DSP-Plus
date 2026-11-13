# Plug in. Speak. Listen.

**Voice DSP+ V2 USB 0.8.9-r4** is a standalone sound card: one processed
microphone channel and one playback channel, both at **48 kHz**. Four physical
microphones feed the XMOS processor; your computer runs your voice application.

[Install USB V2](https://raspiaudio.com/voicedsp/webflasher/?firmware=v2-usb-mono-48k) |
[Download firmware](Firmware/README.md) | [Sources](Sources/README.md) |
[All three setups](../README.md)

## Get started

1. Use a **Voice DSP+ V2** with the matching [USB firmware](Firmware/README.md).
   For an existing USB-enabled V2, [open the updater](https://raspiaudio.com/voicedsp/webflasher/?firmware=v2-usb-mono-48k),
   stop playback, connect and install the preselected **0.8.9-r4** image.
2. Connect its **XMOS USB data port** to your computer with a data cable.
3. Select **Voice DSP+ V2 USB Mono 48k DEV** as both input and output in your app.
   The retained `DEV` name identifies the tested binary.
4. Start at a low computer volume, then increase gradually. No Pi, ZeroCore
   application or Home Assistant is needed during normal USB use.

Audio you want AEC to cancel must play through the Voice DSP+ USB speaker stream.
An independent computer speaker is not automatically an echo reference.

## Everyday controls

| Action | Effect |
| --- | --- |
| UP, short press | Playback volume +3 dB |
| DOWN, short press | Playback volume -3 dB |
| Hold DOWN for 4 seconds | Internal speakers / external speakers |
| Keep UP held | No repeated volume increments |
| Forward gesture | Speaker mute, not microphone mute |
| Any of six directional gestures while muted | Unmute after the 1-second cooldown |
| Right / left gesture | Volume + / - |

Directions are the PAJ sensor event names and depend on its mounting. Wave and
circles do not change playback. Long DOWN does not also lower volume on release.
Buttons and gestures follow the same Windows playback volume and mute.

**Output and volume are remembered:** wait at least 3 seconds after an adjustment
before removing power. Mute is not restored. Empty settings start on internal
speakers at -30 dB. Cold boot and bidirectional Windows controls were tested.

## Pick your microphones and speakers

The four onboard microphones form a square. External LINE/SQUARE ribbon arrays
are detected at startup. Change arrays with power off, then reboot. The long
button switches **speakers**, not microphones. [Wiring and geometry](../Raspberry%20Pi/Documentation/hardware.md).

The two internal speakers reproduce the mono program. External terminals drive
passive full-range speakers or a speaker with its own passive crossover. Use
the jack for a powered speaker/line input, never amplifier terminals.

> **External / Full Speaker require USB-C PD directly on Voice DSP+.**
> A 65 W or higher charger is recommended. Start low: 0 dB is no general
> attenuation, not guaranteed distortion-free power for every speaker/supply.
> Do not feed two power supplies through the shared Voice DSP+/host power header.

Full Speaker splits the same mono program into **L woofer / R tweeter**. Select
it explicitly before playback; External and the long-button toggle are full-range
and unsafe for a bare tweeter. Retain a protective series capacitor and match
the crossover to your drivers.

## Advanced controls

Install Python and `pyusb`, plus a usable libusb backend. From `USB/Sources`:

```sh
python tools/usb_mono_control.py --route internal --db -30 --execute
python tools/usb_mono_control.py --route external --db -30 --execute
python tools/usb_mono_control.py --route full-speaker --db -24 --execute
```

Omit `--execute` to preview. `--help` lists status, mute and optional crossover
settings. Run only one USB control client at a time. On Windows, do not replace
the USB Audio or composite-parent driver to access vendor control/DFU.

## AEC checkpoint

The accepted external-mono test played music on Voice DSP+ and independent speech
on Realtek, with an UMIK recording the room simultaneously. 0.8.9-r4 preserves
the accepted 0.8.8 acoustic tuning while correcting startup volume restoration.
This is not a stereo-AEC or fresh acoustic qualification. Raw versus processed
comparisons include the whole DSP chain; private voice recordings are not included.

[Release provenance and hashes](Firmware/release.json).

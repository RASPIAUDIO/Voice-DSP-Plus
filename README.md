![Voice DSP+ V2: USB, Home Assistant and Raspberry Pi](Raspberry%20Pi/Documentation/assets/voice-dsp-guide.svg)

# Voice DSP+ V2

**Give your project a voice. Choose your microphones, speakers and host.**

Four microphones, onboard voice processing, acoustic echo cancellation, speaker
outputs and hands-free controls. Build a compact voice interface or a two-way
speaker system with your own application.

[Product page](https://raspiaudio.com/product/voicedsp/) |
[USB guide](USB/README.md) |
[Home Assistant guide](Home%20Assistant/README.md) |
[Raspberry Pi guide](Raspberry%20Pi/README.md) |
[Hardware and microphones](Raspberry%20Pi/Documentation/hardware.md)

> **This is the V2 hardware branch.** For the original board, use
> [main](https://github.com/RASPIAUDIO/Voice-DSP-Plus/tree/main).
> Do not mix V1 and V2 firmware or pinouts.

## Choose your setup

| USB | Home Assistant | Raspberry Pi |
| --- | --- | --- |
| Standalone computer sound card | Voice satellite with ZeroCore S3 | Pi4 audio HAT for your application |
| 48 kHz mono input/output, Windows volume, gestures and buttons | Local wake word, Assist, music and on-device settings | 48 kHz audio, desktop controls and your Linux software |
| **0.8.9-r4** | **0.6.75 r8** | **48 kHz installer kit** |
| [Get started](USB/README.md) | [Get started](Home%20Assistant/README.md) | [Get started](Raspberry%20Pi/README.md) |
| [Firmware](USB/Firmware/README.md) / [Sources](USB/Sources/README.md) | [Firmware](Home%20Assistant/Firmware/README.md) / [Sources](Home%20Assistant/Sources/README.md) | [Firmware](Raspberry%20Pi/Firmware/README.md) / [Sources](Raspberry%20Pi/Sources/README.md) |
| [Install USB V2 in your browser](https://raspiaudio.com/voicedsp/webflasher/?firmware=v2-usb-mono-48k) | [Install Home Assistant V2](https://apps.raspiaudio.com/#device=ZeroCoreS3&application=VoiceDSPV2) | [Install on Raspberry Pi](Raspberry%20Pi/README.md) |

**Three ways to use the same audio hardware.** Each folder includes its own
installation guide, matching binaries, sources and checksums. Each guide records
the release's verification scope and remaining physical tests. The Home Assistant
timer-gesture update preserves the previously accepted acoustic settings.
Choose the correct mode: their XMOS images are not interchangeable.
Use desktop Chrome/Edge for the web installers. USB updater: connect the **XMOS**
data port of an already USB-enabled V2. Home Assistant: connect **ZeroCore S3**.
Blank XMOS boards need initial provisioning before USB DFU is available.

[All three installer choices](https://apps.raspiaudio.com/#device=VoiceDSP) |
[Switch USB to host-controlled I2S](https://raspiaudio.com/voicedsp/webflasher/?firmware=v2-i2s-48k)
([requirements and recovery](USB/Firmware/I2S.md)). Switching to I2S removes USB
audio and browser updating; the new update path still needs a hardware test.

## USB: one cable, familiar controls

Connect the XMOS USB data port, select Voice DSP+ as microphone and speaker,
and launch your voice application. Buttons and gestures follow Windows volume
and mute. Hold DOWN for four seconds to switch internal/external speakers;
the last output and volume survive a power cycle. [USB guide](USB/README.md).

## Home Assistant: a connected voice satellite

Add **ZeroCore S3**, install the ESP32 factory image through its USB port, and
configure Wi-Fi. The ESP32 loads its matching XMOS image automatically, without
XTAG. **Voice Compact / Full Speaker / Custom**, audio controls, gestures and
diagnostics appear on the device page; no custom dashboard is required.
Wake word runs locally on ESP32; AEC runs on XMOS; Home Assistant handles the
conversation pipeline. [Home Assistant guide](Home%20Assistant/README.md).

## Raspberry Pi: familiar desktop controls

After installing a complete kit, use the taskbar speaker/microphone icons for
volume and mute, and **Voice DSP+ Output** for **Internal / External / Full Speaker**.
The XMOS handles audio processing; the Pi runs your software.

> **External and Full Speaker require USB-C PD directly on Voice DSP+.**
> A 65 W or higher charger is recommended. Voice DSP+ can power the Pi through
> the header. Power off before rewiring and disconnect the Pi's USB-C supply.
> **Never power both USB-C inputs at once.**

[Getting started](Raspberry%20Pi/README.md) |
[Desktop audio and output guide](Raspberry%20Pi/README.md#4-use-it-like-a-sound-card) |
[Hear the AEC demo](Raspberry%20Pi/README.md#hear-the-aec-on-raspberry-pi-4) |
[AEC, DOA and raw captures](Raspberry%20Pi/Documentation/raspberry-pi-controls.md) |
[40-pin pinout](Raspberry%20Pi/Documentation/pinout.md)

## Make it your own

- **Voice Compact:** small internal speakers and a compact microphone enclosure.
- **External speaker:** suitable 4 or 8 ohm passive full-range speaker, or an
  enclosure with its own passive crossover.
- **Full Speaker:** separate L woofer / R tweeter with a matched active crossover.
  The supplied 9500 Hz preset is for the tested assembly, not every tweeter.
- **Microphones:** four onboard square microphones or an external LINE/SQUARE
  ribbon array detected at startup. Four active microphones, not eight.

## Repository layout

```text
USB/
  README.md        Plug-and-play audio, buttons, gestures and persistence
  Sources/         USB integration, host control, tests and XMOS source changes
  Firmware/        USB update, factory, RAM and XE images with checksums
Home Assistant/
  README.md        Installation, Assist, product modes and daily controls
  Sources/         ESPHome YAML, components, assets and XMOS source changes
  Firmware/        ESP32 factory/OTA and matching XMOS image with checksums
Raspberry Pi/
  README.md        Installation and daily use
  Sources/         Linux controller, installer, desktop UI and tests
  Firmware/        Installer, paired images and checksums
  Documentation/   Wiring, pinout, AEC/DOA controls and release scope
```

The Pi kit is based on the tested Pi4 64-bit Raspberry Pi OS setup. Pi5, other
kernels and speaker assemblies are not qualified by those tests.
[Release scope](Raspberry%20Pi/Documentation/release-notes.md) |
[License and third-party notices](NOTICE.md).
No credentials, private test recordings or proprietary SDK are included.

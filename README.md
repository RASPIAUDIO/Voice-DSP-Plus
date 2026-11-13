![Voice DSP+ V2: USB, Home Assistant and Raspberry Pi](Raspberry%20Pi/Documentation/assets/voice-dsp-guide.svg)

# Voice DSP+ V2

**Give your project a voice. Choose your microphones, speakers and host.**

Four microphones, onboard voice processing, acoustic echo cancellation, speaker
outputs and hands-free controls. Build a compact voice interface or a two-way
speaker system with your own application.

[Product page](https://raspiaudio.com/product/voicedsp/) |
[Raspberry Pi guide](Raspberry%20Pi/README.md) |
[Hardware and microphones](Raspberry%20Pi/Documentation/hardware.md)

> **This is the V2 hardware branch.** For the original board, use
> [main](https://github.com/RASPIAUDIO/Voice-DSP-Plus/tree/main).
> Do not mix V1 and V2 firmware or pinouts.

## Choose your setup

| Raspberry Pi | USB | Home Assistant |
| --- | --- | --- |
| Pi4 audio HAT for your application | Standalone computer sound card | Voice satellite with ZeroCore S3 |
| 48 kHz processed mono capture and mono program playback | USB release to follow | ESPHome/Assist release to follow |
| [Sources, installation and controls](Raspberry%20Pi/README.md) | [USB section](USB/README.md) | [Home Assistant section](Home%20Assistant/README.md) |

**Ready now:** Raspberry Pi installer, paired firmware, Linux sources, desktop
controls and guides. [Download the Pi kit](Raspberry%20Pi/Firmware/README.md).
USB and Home Assistant packaging will follow.

## Raspberry Pi: familiar desktop controls

After installing a complete kit, use the taskbar speaker/microphone icons for
volume and mute, and **Voice DSP+ Output** for **Internal / External / Full Speaker**.
The XMOS handles audio processing; the Pi runs your software.

> **External and Full Speaker require USB-C PD directly on Voice DSP+.**
> A 65 W or higher charger is recommended. Voice DSP+ can power the Pi through
> the header. Power off before rewiring and disconnect the Pi's USB-C supply.
> **Never power both USB-C inputs at once.**

[Getting started](Raspberry%20Pi/README.md) |
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
Raspberry Pi/
  README.md        Installation and daily use
  Sources/         Linux controller, installer, desktop UI and tests
  Firmware/        Installer, paired images and checksums
  Documentation/   Wiring, pinout, AEC/DOA controls and release scope
USB/
  Sources/         Reserved for the USB release
  Firmware/        Reserved for the USB release
Home Assistant/
  Sources/         Reserved for ESPHome and host components
  Firmware/        Reserved for the paired release
```

The Pi kit is based on the tested Pi4 64-bit Raspberry Pi OS setup. Pi5, other
kernels and speaker assemblies are not qualified by those tests.
[Release scope](Raspberry%20Pi/Documentation/release-notes.md) |
[License and third-party notices](NOTICE.md).
No credentials, private test recordings or proprietary SDK are included.

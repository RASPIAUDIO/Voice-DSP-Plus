# Your Home Assistant, with a voice

**Voice DSP+ V2 + ZeroCore S3** brings local wake-word detection, music, spoken
answers and hands-free controls to Home Assistant. Use your own speakers or
the corresponding Voice Compact / Full Speaker assembly.

[Download 0.6.61](Firmware/README.md) | [ESPHome sources](Sources/README.md) |
[All three setups](../README.md)

## Install in five steps

1. With power off, mount **RASPIAUDIO ZeroCore S3** on **Voice DSP+ V2**.
   You need Home Assistant, 2.4 GHz Wi-Fi and a USB data cable. Remove any Pi.
2. Connect the **ZeroCore USB data port**, not the XMOS port, to your computer.
3. Download the [factory image](Firmware/voicedsp-v2-0.6.61.factory.bin).
   Open [ESPHome Web](https://web.esphome.io/) in Chrome/Edge, connect, choose
   Install, then select this file. An erased install removes saved settings.
4. Configure Wi-Fi using serial Improv through the installer. Alternatively use
   the fallback **VoiceDSP V2 Setup** access point (password `voicedsp-setup`).
   This particular image does not advertise BLE Improv.
5. Add the discovered ESPHome device in Home Assistant. Wait for **Audio Controller
   Ready** and **XMOS firmware status: Ready**, select your Assist pipeline, then
   try **Hey Jarvis**. Start with a low playback volume.

[RASPIAUDIO Apps](https://apps.raspiaudio.com/) is another installation entry point.
Its catalog is deployed separately: verify **V2 0.6.61** before using it. The
downloadable factory file above is the exact release in this branch.

## Everything on the device page

No custom dashboard or admin token is required. Open the ESPHome device in HA.

| Product Mode | Best for |
| --- | --- |
| **Voice Compact** | Small internal speakers and a compact voice interface |
| **Full Speaker** | Separate external L woofer / R tweeter with a matched crossover |
| **Custom** | Explicit output, level and tone settings |

Select a mode, press **Apply Product Mode**, then **Save Mode Settings** to
remember adjustments. Internal/external sliders cover **-60 to 0 dB**, in 3 dB
steps. Zero means no general DAC/analog attenuation; player volume, amplifier
gain and visible per-driver filter trims remain independent.

Daily controls include speaker/microphone mute, output, bass/treble, gesture
enable (default ON), wake sensitivity and Stop behavior. Buttons, gestures,
startup/wake/mute sounds and listening/thinking/replying LED states are retained.
Gesture mute is **speaker mute**, not microphone privacy mute.

Geometry, power diagnostics, amplifier fault, firmware status and troubleshooting
controls remain available. **Show Advanced Settings / Show Diagnostics** are
helper entities; the standard HA device page does not dynamically collapse its
entity list. No radar or ambient-light sensor is claimed.

> **External / Full Speaker require USB-C PD directly on Voice DSP+.**
> A 65 W or higher supply is recommended. Do not feed multiple supplies through
> the shared power header. Full Speaker: **L = woofer, R = tweeter**. Retain a
> protective series capacitor. The editable 9500 Hz / L 0 dB / R -6 dB preset is
> for the tested speaker, not every tweeter. Never select unfiltered External for
> a bare tweeter.

## Who does what?

- **ESP32:** local Hey Jarvis, Home Assistant connection and media.
- **XMOS:** four-microphone processing, AEC, noise suppression and LED DOA.
- **Home Assistant:** your speech-to-text, conversation and TTS pipeline.

Speech services can be local or cloud-based, depending on your HA configuration.
Internal square or external LINE/SQUARE arrays are selected at startup.
[Hardware and wiring](../Raspberry%20Pi/Documentation/hardware.md).

## Updates and recovery

The ESP32 image contains its matching XMOS firmware. Each boot loads it into
**XMOS RAM**, leaving QSPI unchanged. No XTAG is required for this normal path.
Do not substitute the standalone USB firmware for the paired I2S image.

**Reload embedded XMOS firmware** restarts the paired boot. **XMOS loading
progress**, **XMOS firmware status** and **Embedded XMOS image SHA256** report it.
The update entity and Beta Firmware selector retain existing remote catalog
URLs; this Git publication does not promote those channels. Use the
[manual OTA instructions](Firmware/README.md) for this exact version.

If the controller remains unavailable, stop audio and remove all power for ten
seconds. Do not add a second I2C owner or repeatedly reload a blocked bus.

This accepted development release retains unencrypted LAN API/OTA and opt-in
capture diagnostics. Use a trusted LAN, never expose its ports to the Internet.
No home Wi-Fi credentials or private recordings are included. AEC and filter
parameters remain those of the accepted HA checkpoint, not later Pi profiles.

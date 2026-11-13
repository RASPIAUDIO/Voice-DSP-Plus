[Voice DSP+ V2](../README.md) / Raspberry Pi

# Give your Raspberry Pi a voice

**Four microphones. Your choice of speakers. Familiar desktop audio controls.**

Voice DSP+ performs microphone processing and AEC; your Pi runs the application.
The 48 kHz kit provides processed mono capture, mono program playback and the
validated AEC policy on internal and external outputs.

**[Download the installer](Firmware/voicedsp-v2-pi48-r8.run)** |
[Checksums and firmware](Firmware/README.md) | [Source code](Sources/README.md)

## 1. Connect

![Voice DSP+ powers the Pi through the header; use one PD supply](Documentation/assets/pi-connection.svg)

Use a **Pi4 Model B with 64-bit Raspberry Pi OS** and a normal desktop user.
Power off, align the 40-pin header and mount Voice DSP+ V2. Remove ZeroCore S3:
the Pi must be the only bus host. See the [pinout](Documentation/pinout.md).

> **External / Full Speaker: USB-C PD directly on Voice DSP+ is required.**
> Use a 65 W or higher charger offering a 20 V profile and a suitable cable.
> Voice DSP+ supplies the Pi through the header. Disconnect the Pi USB-C supply
> before powering on. Never connect both USB-C supplies at once.

## 2. Download

Download the installer and checksum on the Pi:

```sh
curl -fL 'https://raw.githubusercontent.com/RASPIAUDIO/Voice-DSP-Plus/V2/Raspberry%20Pi/Firmware/voicedsp-v2-pi48-r8.run' -o voicedsp-v2-pi48-r8.run
curl -fL 'https://raw.githubusercontent.com/RASPIAUDIO/Voice-DSP-Plus/V2/Raspberry%20Pi/Firmware/voicedsp-v2-pi48-r8.sha256' -o voicedsp-v2-pi48-r8.sha256
sha256sum -c voicedsp-v2-pi48-r8.sha256
```

The installer contains the paired image, host sources and software tests.
No Git account, XMOS SDK or XTAG is needed.

## 3. Install and reboot

On a fresh Pi audio setup:

```sh
sudo bash ./voicedsp-v2-pi48-r8.run --confirm-v2-exclusive --desktop --reboot
```

Save your work first. This installs dependencies, compiles/tests the controller,
adds the overlay and reboots. Setup continues after reboot with audio discovery
and AEC readback verification. Existing installations or conflicting overlays
are preserved rather than overwritten.

```sh
journalctl -u voicedsp-v2-48k-setup.service -b --no-pager
```

Look for **`SUCCESS: accepted AEC verified`**. XMOS loads into RAM at each boot;
QSPI is not rewritten. This is an audio kit, not a bundled wake-word or HA app.

## 4. Use it like a sound card

- **Speaker / microphone icons:** left-click for volume/mute; right-click for
  device selection. It may appear as **VoiceDSP V2 I2S**.
- **Voice DSP+ Output:** choose Internal, External or Full Speaker from the
  tray or application menu. External modes ask for PD wiring confirmation.
- **Volume:** internal hardware gain is 0 dB; desktop volume starts at 25%
  and is subsequently remembered. Start quietly.

```sh
arecord -r 48000 -c 1 -f S16_LE -d 8 my-voice.wav
aplay my-voice.wav
```

| Output | What to connect |
| --- | --- |
| Internal | Two small internal speakers, tested with 8 ohm loads |
| External | Full-range passive speaker or enclosure with passive crossover |
| Full Speaker | Matched L woofer / R tweeter, validated active crossover |

**Never select unfiltered External for a bare tweeter.** Full Speaker's LR4
9500 Hz / 0 and -6 dB trims belong to the tested assembly, not a universal preset.
At core boot the output is OFF/-30 dB; desktop startup then selects Internal
at hardware 0 dB. The route itself is not persisted by this Pi kit.

## Controls and development

[AEC / DOA / raw captures](Documentation/raspberry-pi-controls.md) |
[Arrays and wiring](Documentation/hardware.md) |
[40-pin pinout](Documentation/pinout.md) |
[Build/test sources](Sources/README.md) |
[Release scope](Documentation/release-notes.md)

Numeric DOA and arbitrary NS/NN tuning are not yet exposed in the Pi CLI.
Raw capture offers channels0/1, not four simultaneous raw microphones.

## Existing kit or headless Pi?

For a desktop-only update, stop playback and run from the `Raspberry Pi` folder
of this checkout:

```sh
sudo python3 Sources/scripts/desktop.py install --user "$USER"
```

This updates managed desktop files without reflashing/replacing the controller.
Audio services restart briefly. Relaunch Voice DSP+ Output or log out/in.
For a new headless kit, omit `--desktop`; select the output manually afterwards.

To remove desktop integration first, then the core (managed files must be unchanged):

```sh
sudo python3 /opt/voicedsp-desktop/desktop.py remove
sudo systemctl stop voicedsp-v2-48k.service
sudo python3 /opt/voicedsp-v2-48k/scripts/install.py uninstall --execute --confirm-v2-exclusive
```

Reboot after removing the overlay. No other audio driver is automatically removed.

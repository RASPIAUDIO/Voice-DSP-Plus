# Raspberry Pi V2 r8: scope and validation

This release packages the existing Pi4 48 kHz audio integration, not a new DSP
algorithm. The portable controller, AEC implementation, codec filter and XMOS
images are preserved from the tested baseline.

## Included and exercised

- Pi4 Model B, 64-bit Raspberry Pi OS: native build, installation on a fresh SD,
  reboot, shared ALSA playback and capture.
- Processed mono microphone and mono program playback at 48 kHz.
- Internal speakers and Full Speaker AEC: matched raw/processed captures with
  independent room-microphone controls; accepted on the tested assemblies.
- Desktop default microphone/speaker, native panel icons and output selector.
- r8 confirmation before External or Full Speaker: USB-C PD directly on the board,
  no simultaneous Pi USB-C supply. Internal remains directly selectable.

Core startup is OFF/-30 dB; desktop activation selects Internal at hardware
0 dB and restores desktop volume (25% initially). Route/gain are volatile on Pi.
Do not confuse configuration verification with a new acoustic measurement.

## AEC and two-way filter

Default profile4 applies to internal and external routes: NS0.10, NN0.51,
ATTNS off, reference/asr routing and exact readbacks in
[accepted-aec.json](../Sources/accepted-aec.json). The previous internal profile6
is available for diagnostics, but is not the validated default.

Full Speaker uses LR4 9500 Hz, woofer trim0 dB, tweeter trim-6 dB. This is a
tested assembly's preset, not a general setting for all tweeters. Choose flat
External only for a full-range speaker or an enclosure with its own passive filter.

## Limits

Pi5, other OS/kernel combinations, long-term endurance, maximum acoustic output
and other enclosures are not qualified by these tests. The kit does not install
Home Assistant, a wake-word model or a speech-transcription service.

Numerical DOA telemetry and arbitrary AEC parameter editing are not exposed in
the current Pi CLI. Raw I2S capture offers two slots, not four simultaneous raw
microphones. See [commands and limitations](raspberry-pi-controls.md).

Raw field recordings and private bench archives are not bundled. The public
source includes the controller/software tests and can reproduce the installation
without access to a private repository. Third-party dependencies keep their
normal notices; no complete proprietary XMOS SDK is included.

## Public package verification, 2026-11-13

The exported package compiled on Pi4: **3 native tests and 53 Python tests passed**.
The self-extracting installer's checksum, extraction and `--help` entry point
were exercised without installing over the running system. Source/package
parity, local documentation links and a private-data scan passed.

This packaging check did not reflash the live board, restart audio, replay the
AEC benchmark or repeat a fresh-SD installation. Acoustic controller sources
match the tested reference (line-ending normalization only); policy values and
paired firmware hashes are unchanged.

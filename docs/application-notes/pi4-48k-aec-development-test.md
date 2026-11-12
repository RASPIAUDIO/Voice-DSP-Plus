# Raspberry Pi 4 / 48 kHz AEC development test

Date: 2026-11-12. **Experimental V2 hardware; not a released firmware or an
installation guide for the currently published Voice DSP+ hardware.**

This note preserves the unsuccessful first acoustic test and a corrected
follow-up below. It does not change the public Raspberry Pi 16 kHz installer,
USB firmware recommendations or previously published validation results.

## Initial setup and method

We followed the comparison structure of the [existing AEC proof](../../advanced/aec-proof/README.md):
voice only, playback echo only, then simultaneous voice and playback, with raw
and processed microphone recordings at fixed source levels.

- Raspberry Pi 4, 48 kHz I2S, prototype V2 board with four microphones.
- Beat played by the two small INTERNAL speakers at -12 dB; external amplifier off.
- English speech played independently on the PC's Realtek speakers, fixed volume.
- Simultaneous UMIK-1 recording as an independent acoustic witness.
- Twelve seconds of echo preadaptation before the 19.644-second speech excerpt.
- Six 38-second device recordings, with complete unnormalized originals retained privately.

RAW selects a post-gain microphone before the processing chain. The comparison
is **one raw microphone versus the full AEC/beamforming/NS/AGC chain**, not an
AEC-only ablation. SHF_BYPASS was never enabled, following the earlier warning
about repeated audio interruptions with that bypass.

The existing V2 internal profile was preserved: NS 0.15, NN 0.51, ATTNS off,
echo and nonlinear suppression on, DT sensitivity 0, AGC on, microphone gain 10,
reference gain 1.5 and codec digital reference enabled. This differs from the
legacy public profile (NS 0.10, DT sensitivity 10, AGC off); it is not presented
as a reproduction of identical legacy tuning or hardware.

## What passed

The Pi installer, automatic SPI RAM loading after reboot, playback and capture
were exercised on hardware. An initial campaign had ALSA capture overruns and
was rejected. Increasing only the capture buffer from 1024 to 8192 frames fixed
that issue; the playback/reference path was unchanged.

The repeated campaign had no reported ALSA xruns, Windows audio callback errors
or digital clipping. The UMIK recordings confirmed both acoustic sources in the
mixed takes, with essentially identical room RMS levels in the raw and processed
conditions. Analog distortion and absolute SPL were not characterized.

## What did not pass initially

**The initial internal-speaker double-talk test failed.**
Speech recognition worked similarly with and without processing when speech was
alone, but failed on the processed mixed recording.

| Condition | Automatic transcript WER |
| --- | ---: |
| Voice alone, RAW | 14.3% |
| Voice alone, processed | 14.3% |
| Voice + beat, RAW | 11.4% |
| Voice + beat, processed | 100.0% |

These figures use local Whisper small.en against its automatic transcript of the
digital source, not a manually corrected reference. They are diagnostic figures,
not directly comparable with results from other recognition models or excerpts.
The processed mixed output lost the target sentence despite its presence on UMIK.

The processed voice-plus-beat window was about 11.7 dB below processed voice alone.
Echo-only attenuation cannot therefore establish success: suppressing the wanted
voice is not acceptable AEC. The reported AEC convergence flag also remained zero;
this was investigated in the follow-up below.

## Corrected follow-up, same day

The Pi host now applies the existing voice-preserving profile to internal
speakers as well as the external path: NS0.10, NN0.51, ATTNS off, DT10, AGC off,
and post echo suppression off. Linear AEC remains active. No XMOS firmware image
or persistent flash change was needed for this host-policy correction.

Paired microphone/reference recordings verified an unclipped reference matching
the actual mono playback at normalized correlation0.997 on both output paths.
The existing reference selection and system delay were retained; correlation
lag alone was not treated as a delay calibration.

| Retained campaign | RAW mixed WER | Processed voice-only WER | Processed mixed WER |
| --- | ---: | ---: | ---: |
| Internal speakers, -6 dB | 22.86% | 14.29% | 11.43% |
| External two-way Full Speaker, -12 dB | 80.00% | 14.29% | 11.43% |

Each campaign contains the same six source/control conditions and simultaneous
UMIK witness recordings. Realtek was fixed at74% and unmuted. The internal test
followed70 seconds of adaptation; convergence was observed after about9.4 seconds.
A partial repeat with an ALSA overrun was rejected and retained as invalid.
The clean internal repeat recorded to RAM before writing the archive, avoiding
SD-card writes in the capture path; this is not proof of the overrun's root cause.
An additional mixed-only external check on the final installed host also passed.

Wanted speech was preserved: processed mixed and voice-only speech windows had
similar RMS levels. Voice-matched full-chain echo-only reduction was20.76 dB
internally and40.01 dB externally. These are **not isolated AEC ERLE**: the raw
and processed paths differ in beamforming, noise suppression and gain as well.
All retained takes passed digital clipping, callback and ALSA XRUN checks.

This is a successful instrumented bench result, not product-wide qualification.
The WER reference remains an automatic35-word transcript, not manual ground truth;
small WER differences reflect recognition variability. Owner listening acceptance,
analog distortion, calibrated SPL and endurance remain outside this result.

## Availability

Sources, exact readbacks, logs and recordings are archived in the private
development repository. **No V2 firmware, binary, private source bundle or test
recording is released by this note.** Do not install the legacy public firmware
on a prototype V2 on the strength of this report.

# AEC proof checkpoint

This checkpoint records the matched Voice DSP+ AEC measurements performed on
2026-07-24. It includes the exact runtime parameters and level-matched listening
extracts. The unmodified 32-bit captures remain outside the repository because
they are test working data, not release firmware.

## Hardware and signal paths

- Firmware: Raspberry Pi I2S, 16 kHz, two processed microphone channels,
  automatic LINE/SQUARE geometry.
- XVF3800 playback reference: Raspberry Pi I2S playback with
  `I2S_DAC_DSP_ENABLE=1`.
- Far-end signal to cancel: deterministic hip-hop beat played by the Voice
  DSP+ onboard speaker.
- Near-end signal to keep: 19.644 s English speech played by the Windows
  Conexant speakers.
- Raspberry Pi sink: `0.50`.
- Windows Conexant output: `0.70`, reported as `-5.37 dB`.
- Windows Conexant microphone: `1.00`.
- Capture: `hw:2,0`, 16 kHz, stereo, signed 32-bit PCM, 30 seconds.
- AEC pre-adaptation: 10 seconds of far-end beat before the ON capture.
- Analysis window: channel 1, seconds 3 through 23.
- STT: Whisper `large-v3`, English.

The OFF reference keeps the normal scheduler active and selects post-gain
microphones. This avoids the rapid dropouts caused by the test-only
`SHF_BYPASS=1` path.

## AEC ON parameters

```text
SHF_BYPASS=0
I2S_DAC_DSP_ENABLE=1
AEC_FAR_EXTGAIN=0
PP_MIN_NS=0.10
PP_MIN_NN=0.51
PP_ATTNS_MODE=0
PP_ECHOONOFF=1
PP_NLATTENONOFF=1
PP_NLAEC_MODE=0
PP_DTSENSITIVE=10
PP_GAMMA_E=1
PP_GAMMA_ETAIL=1
PP_GAMMA_ENL=1.1
PP_AGCONOFF=0
PP_AGCGAIN=10
PP_LIMITONOFF=1
PP_LIMITPLIMIT=0.47
AUDIO_MGR_MIC_GAIN=10
AUDIO_MGR_REF_GAIN=1.5
AUDIO_MGR_OP_L=8 0
AUDIO_MGR_OP_R=8 1
```

The ON runs ended with `AEC_AECCONVERGED=1` and `AEC_AECPATHCHANGE=0`.

## Clean AEC OFF reference

```text
SHF_BYPASS=0
I2S_DAC_DSP_ENABLE=1
AEC_FAR_EXTGAIN=0
PP_ECHOONOFF=0
PP_NLATTENONOFF=0
AUDIO_MGR_MIC_GAIN=10
AUDIO_MGR_OP_L=3 0
AUDIO_MGR_OP_R=3 1
```

Mux category 3 is the post-gain microphone path. OFF is therefore a clean
capture reference, not the unstable firmware-wide SHF bypass.

## Results

The expected sentence contains 35 words.

| Run | Conexant proof level OFF / ON | AEC OFF | AEC ON | Convergence |
| --- | --- | ---: | ---: | --- |
| RERUN7 | -44.44 / -45.73 dBFS | 44 errors, 125.7% WER | 2 errors, 5.7% WER | 1 |
| RERUN8 | -45.82 / -45.36 dBFS | 2 errors, 5.7% WER | 2 errors, 5.7% WER | 1 |

RERUN7 is the clear double-talk proof: without AEC, the beat prevents correct
recognition; with AEC, the 35-word near-end sentence remains intelligible.
RERUN8 is an important repeatability guardrail: both paths were intelligible,
so AEC did not improve WER in that acoustic realization, but it also did not
damage the transcription. A single run must not be presented as a universal
echo-reduction figure.

## Listening extracts

- [RERUN7 AEC OFF, level matched](audio/rerun7-aec-off-level-matched.wav)
- [RERUN7 AEC ON, level matched](audio/rerun7-aec-on-level-matched.wav)

Both files contain the same 20-second aligned analysis window. They are mono,
16 kHz, 16-bit PCM and independently normalized to -20 LUFS for convenient
listening. Use the original captures, not these normalized files, for level or
echo-reduction measurements.

## Reproduction

Use [../raspberry-pi-aec-profile.sh](../raspberry-pi-aec-profile.sh) to apply
the profiles and [../raspberry-pi-aec-capture.sh](../raspberry-pi-aec-capture.sh)
to record direct ALSA captures. The deterministic beat can be regenerated with
[../tools/generate_aec_hiphop_beat.py](../tools/generate_aec_hiphop_beat.py),
and capture continuity can be checked with
[../tools/analyze_aec_pair.py](../tools/analyze_aec_pair.py).

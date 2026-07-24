# AEC double-talk validation - 2026-07-23

The repeated 2026-07-24 measurement, exact runtime parameters and listening
extracts are archived in [aec-proof/README.md](aec-proof/README.md).

## Result status

The corrected matched test is valid. The clean AEC OFF reference keeps the
normal scheduler active and exposes post-gain microphones through audio-manager
mux category 3. It has no repeating click or zero-filled dropout. The selected
AEC ON candidate keeps the 35-word near-end speech intelligible while Voice
DSP+ plays the hip-hop far-end signal.

| Matched capture | Word errors | WER | RMS | Peak | Clipped samples |
| --- | ---: | ---: | ---: | ---: | ---: |
| Clean post-gain microphones, AEC OFF | 4 / 35 | 11.4% | -43.90 dBFS | -19.77 dBFS | 0 |
| Full processing, AEC ON | 1 / 35 | 2.9% | -22.82 dBFS | -1.69 dBFS | 0 |

The clean AEC OFF transcript was:

```text
It has all a shift towards a world without laws, where international law is
trampled underfoot, and where the only law that seems to matter is that of the
strongest, and imperial ambitions are resurfacing.
```

The matched AEC ON transcript was:

```text
It's as though a shift towards a world without rules, where international law
is trampled underfoot and where the only law that seems to matter is that of
the strongest, and imperial ambitions are resurfacing.
```

The RMS values are included as capture diagnostics, not as a direct echo
reduction measurement: mux category 3 is a raw post-gain path while category 8
is a processed output with different gain and beamforming.

## Selected runtime profile

```text
SHF_BYPASS=0
AEC_FAR_EXTGAIN=0
AEC_AECCONVERGED=1
PP_ECHOONOFF=1
PP_NLATTENONOFF=1
PP_NLAEC_MODE=0
PP_DTSENSITIVE=10
PP_GAMMA_E=1
PP_GAMMA_ETAIL=1
PP_GAMMA_ENL=1.1
PP_MIN_NS=0.10
PP_MIN_NN=0.51
PP_ATTNS_MODE=0
PP_AGCONOFF=0
PP_AGCGAIN=10
PP_LIMITONOFF=1
PP_LIMITPLIMIT=0.47
AUDIO_MGR_MIC_GAIN=10
AUDIO_MGR_REF_GAIN=1.5
```

`PP_DTSENSITIVE=10` is the retained double-talk compromise. It preserves the
near-end voice better than the more aggressive AEC variants tested. Reducing
`PP_AGCGAIN` from 80 to 10 removed the digital clipping observed in the first
otherwise successful candidate.

## Matched test protocol

- Near-end source: `davos_macron_playback.wav`, played by the Windows Conexant
  speakers.
- Far-end source: `hiphop_aec_source_half_minus20pct_safe.wav`, played by the
  Voice DSP+ speaker through Raspberry Pi I2S.
- Voice DSP+ playback sink: 50%, reported as -18.06 dB.
- Capture: `hw:2,0`, 16 kHz, stereo, signed 32-bit PCM.
- The beat starts before the near-end speech to let the AEC adapt.
- The board, source levels and acoustic geometry were unchanged between OFF
  and ON captures.
- Whisper: `large-v3`, CUDA, English, temperature 0, beam size 20, patience 2,
  and `condition_on_previous_text=False`.

The earlier rejected OFF capture used `SHF_BYPASS=1`, `PP_ECHOONOFF=0`, and
`PP_NLATTENONOFF=0`. `SHF_BYPASS` switches the firmware to the dedicated raw
pipeline bypass and processor burn path. It is not suitable as the clean OFF
reference on this build because that path produces audible dropouts. Objective
continuity analysis found 1,090 large sample discontinuities and 5,008
transitions into or out of zero. The corrected post-gain OFF capture found zero
of either event.

## Clean OFF implementation

Keep `SHF_BYPASS=0` so the normal frame scheduler remains active. Use the
audio-manager output mux to expose clean microphone signals without switching
the entire far-field pipeline:

```text
AUDIO_MGR_OP_L 3 0
AUDIO_MGR_OP_R 3 1
```

Category 3 is microphone data after the common microphone gain. If a strict
raw pre-gain reference is required, use category 1 instead. This keeps
`SHF_BYPASS=0` and avoids the clicking test-only bypass path.

A second useful comparison can keep the linear AEC active but disable only
post-AEC echo suppression:

```text
SHF_BYPASS=0
PP_MIN_NS=1.0
PP_MIN_NN=1.0
PP_ECHOONOFF=0
PP_NLATTENONOFF=0
PP_AGCONOFF=0
AUDIO_MGR_OP_L 7 0
AUDIO_MGR_OP_R 7 1
```

This second profile exposes AEC residuals. It must be labelled `linear AEC
only`, not `AEC OFF`.

## Local evidence

The large WAV files remain under the ignored local benchmark directory:
`aec_test/davos_benchmark_2026-07-23/`.

```text
MATCHED_CLEAN_AEC_OFF_POSTGAIN.wav
MATCHED_AEC_ON_DT10_GAIN10.wav
LISTEN_MATCHED_OFF_NORMALIZED.wav
LISTEN_MATCHED_ON_NORMALIZED.wav
matched_clean_off_vs_on_metrics.json
matched_clean_off_vs_on_whisper.json
```

The board was left on the selected AEC ON candidate above, with
`AEC_AECCONVERGED=1` confirmed after the final capture.

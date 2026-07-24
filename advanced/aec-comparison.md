# Voice DSP+ AEC comparison

This test isolates acoustic echo cancellation from the validated noise
suppression profile. The Voice DSP+ playback path plays a deterministic,
bass-heavy hip-hop beat, so the XVF3800 receives the exact far-end reference.
A second loudspeaker plays a repeatable speech file that must remain audible in
the processed microphone output.

## Controlled profiles

All profiles keep `PP_MIN_NS=0.10`, `PP_MIN_NN=0.51`, `PP_ATTNS_MODE=0`, AGC
and the limiter unchanged.

| Profile | Captured path | Echo post-processing | Non-linear attenuation |
| --- | --- | --- | --- |
| `raw` | post-gain microphones, mux category 3 | off | off |
| `linear` | linear AEC residuals, mux category 7 | on | off |
| `full` | processed output, mux category 8 | on | on |

The required comparison is `raw` versus `full`. Do not set `SHF_BYPASS=1` for
the OFF reference: that dedicated timing-test path produces repeating dropouts
on this build. Use `linear` only to identify whether audible speech damage
comes from non-linear attenuation.

Generate the repeatable beat:

```bash
python advanced/tools/generate_aec_hiphop_beat.py \
  --output aec_hiphop_beat_16k.wav
```

Install the Raspberry Pi helpers:

```bash
sudo install -m 755 advanced/raspberry-pi-aec-profile.sh \
  /usr/local/bin/voice-dsp-plus-aec-profile
sudo install -m 755 advanced/raspberry-pi-aec-capture.sh \
  /usr/local/bin/voice-dsp-plus-aec-capture
```

For every pass, keep the loudspeakers, master volumes and microphone positions
unchanged. Apply the profile before arming the external speech playback. Profile
application performs several I2C transactions and must not run inside the timed
capture window. Start the external speech capture/playback with a five-second
countdown, then immediately start the Voice DSP+ capture with profile application
disabled:

```bash
voice-dsp-plus-aec-profile apply raw
# Arm the external speech source here, with a five-second playback delay.
VOICE_DSP_SKIP_PROFILE_APPLY=1 voice-dsp-plus-aec-capture \
  raw aec_hiphop_beat_16k.wav aec-raw.wav

voice-dsp-plus-aec-profile apply full
# Arm the same external speech source again with the same delay.
VOICE_DSP_SKIP_PROFILE_APPLY=1 voice-dsp-plus-aec-capture \
  full aec_hiphop_beat_16k.wav aec-full.wav
```

Retain the raw files. Do not normalize them independently before checking echo
reduction, clipping and speech damage.

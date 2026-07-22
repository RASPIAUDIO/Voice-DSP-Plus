# Voice DSP+ noise-suppression comparison

## Recommended Raspberry Pi settings

The best subjective result validated on 2026-07-22 for the Raspberry Pi
16 kHz I2S voice profile is:

```text
PP_MIN_NS=0.10
PP_MIN_NN=0.51
PP_ATTNS_MODE=0
```

AEC, non-linear echo attenuation, AGC and the limiter remain enabled. This
profile provided a good reduction of stationary background noise without the
slow speech attacks and audible level fluctuations produced by aggressive
ATTNS gating.

Install and apply it on a Raspberry Pi with Voice DSP+ already configured:

```bash
sudo install -m 755 advanced/raspberry-pi-ns-profile.sh \
  /usr/local/bin/voice-dsp-plus-ns-profile
voice-dsp-plus-ns-profile apply recommended
```

These commands write the XVF3800 controls over I2C at runtime. They do not
replace the firmware image. Reapply the profile after a reboot unless a startup
service or a future firmware release embeds the same defaults.

### Required baseline

Every tuning session must also capture `no-processing` under the same acoustic
conditions. It keeps the beam and audio routing unchanged but sets both noise
floors to `1.0` and disables AEC, non-linear attenuation, AGC, ATTNS and the
limiter:

```bash
voice-dsp-plus-ns-profile apply no-processing
arecord -D pulse -f S32_LE -r 16000 -c 2 -d 10 baseline-no-processing.wav
voice-dsp-plus-ns-profile apply recommended
```

Do not compare recordings made with different loudspeaker positions, playback
levels or background-noise levels. Level-matched listening files are useful,
but the untouched captures must also be retained for clipping and gain checks.

This test records Voice DSP+ and the computer microphone at the same time while
controlled pink noise is played through the computer speakers.

This setup evaluates noise suppression and beamforming. It does not evaluate
AEC because the noise is not played through the Voice DSP+ playback path.

## Physical setup

1. Put Voice DSP+ beside the computer microphone.
2. Keep both microphones at the same distance from the talker and noise speaker.
3. Start with the computer speaker volume low.
4. Disable microphone enhancements for the Conexant input when possible.
5. Do not normalize the two recordings independently before comparing them.

An external speaker connected to the Conexant output is fairer than laptop
speakers because it can be placed at the same distance from both microphones.

## Run on Windows

Install the two Python dependencies once:

```powershell
python -m pip install numpy sounddevice
```

Run the comparison from the repository root:

```powershell
python advanced\tools\compare_noise_suppression.py
```

The protocol is:

1. Synchronization tone and settling time: remain silent.
2. Noise only: remain silent.
3. Two beeps: start speaking naturally after the cue.
4. Noise plus speech: keep speaking continuously.
5. One beep: the noise has stopped; keep speaking after the cue.
6. Clean speech: continue until the test finishes.

Generated files are stored in `noise_test/`:

- `voice_dsp_plus.wav`
- `conexant.wav`
- `comparison_left_voice_dsp_right_conexant.wav`
- `comparison_ab.wav`
- `metrics.json`

The reported contrast is clean-speech RMS minus noise-only RMS. This reduces the
effect of different microphone gains, but it remains a demonstration metric and
not a laboratory SNR measurement.

For an AEC comparison, repeat a separate test with the interfering audio played
through Voice DSP+ itself so its DSP receives the exact playback reference.

## Repeatable voice with an external noise source

Use this mode when the background noise comes from a separate physical source
and a known voice file is played through the computer speakers:

```powershell
python advanced\tools\compare_external_noise.py "C:\path\to\voice.mp3"
```

The output includes raw files and level-matched listening files. Level matching
applies one constant gain to each complete recording, so it does not alter that
microphone's voice-to-background ratio.

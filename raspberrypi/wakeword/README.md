# Hey Jarvis far-field demo

This optional demo runs the pretrained `hey_jarvis` model on the **Raspberry Pi
CPU** using the processed 16 kHz Voice DSP+ microphone stream. The model is not
embedded in or executed by the XMOS XVF3800. The XVF3800 supplies the far-field
audio front end and this application does not add another noise-suppression
stage after its DSP.

See the [far-field wake-word application note](../../docs/application-notes/far-field-wake-word-demo.md)
for the complete XMOS-versus-Raspberry-Pi architecture and validated demo.

## Audio prerequisite

Use the validated Raspberry Pi I2S 16 kHz AUTO firmware. On the reference Pi,
the boot service is:

```bash
sudo systemctl enable --now pi-ai-mic-rpi-16k-auto-spi-boot.service
arecord -D hw:2,0 -f S32_LE -r 16000 -c 2 -d 3 /tmp/voice-dsp-check.wav
```

The ALSA capture must contain non-zero samples before installing the detector.

## Install

```bash
mkdir -p ~/voice-dsp-hey-jarvis ~/.config/systemd/user
python3 -m venv ~/voice-dsp-hey-jarvis/.venv
~/voice-dsp-hey-jarvis/.venv/bin/pip install pyopen-wakeword==1.1.0

cp hey_jarvis_detector.py ~/voice-dsp-hey-jarvis/
cp apply_xmos_profile.sh ~/voice-dsp-hey-jarvis/
chmod +x ~/voice-dsp-hey-jarvis/apply_xmos_profile.sh
cp voice-dsp-hey-jarvis.service ~/.config/systemd/user/

# Disable the previous experimental detector because ALSA capture is exclusive.
systemctl --user disable --now voice-dsp-wakeword.service 2>/dev/null || true
systemctl --user daemon-reload
systemctl --user enable --now voice-dsp-hey-jarvis.service
```

The service uses:

- processed input `plughw:2,0`, mono, signed 16-bit PCM, 16 kHz;
- pretrained `hey_jarvis` model;
- threshold `0.35` (slightly reduced sensitivity);
- one model hit per detection;
- 1.5 second application cooldown;
- a short 880 Hz confirmation beep on the Voice DSP+ output; its playback
  temporarily uses 50% sink volume, then restores the previous volume;
- an amber user LED indication for two seconds, followed by restoration of the
  normal red=jack and green=square status;
- continuous model windows; the optional reset mode is kept for diagnostics only;
- no additional Speex noise suppression.

Before each detector start, `apply_xmos_profile.sh` waits for XVF3800 host
control and applies the validated far-field front-end profile:

- `PP_MIN_NS=0.10`, `PP_MIN_NN=0.51`;
- AGC enabled and seeded at `PP_AGCGAIN=10`;
- `AUDIO_MGR_MIC_GAIN=10`;
- limiter enabled with `PP_LIMITPLIMIT=0.47`.

`PP_AGCGAIN` is the live adaptive gain: its readback is expected to move after
startup. The value `10` is an initial clean seed, not a fixed gain target.

## Verify

```bash
systemctl --user status voice-dsp-hey-jarvis.service
journalctl --user-unit=voice-dsp-hey-jarvis.service -f -o cat
```

Say "Hey Jarvis" with a short pause after the phrase. A successful event looks
like:

```text
DETECTED model=hey_jarvis score=0.982 rms=0.0843 time=2026-07-26T17:52:17
```

Disable audible and LED feedback for measurement runs with `--no-feedback`.
The feedback runs on a separate worker and does not pause microphone capture or
wake-word inference.

Hardware checkpoint validated on 2026-07-26: detection changes the bicolor LED
immediately, plays the short confirmation beep without the previous DAC startup
delay, restores the normal jack/micro-pattern indication, and returns the
speaker sink to its previous volume.

## Remote Windows test

`windows_test_hey_jarvis.ps1` uses the default Windows speaker and repeats the
phrase. `windows_run_hey_jarvis_test.cmd` starts it in the logged-in desktop
session, which is required because an SSH service session normally has no
physical audio output.

For a repeatable human-voice test, copy `jarvis.ogg` to the Windows desktop and
install the playback dependencies:

```powershell
python -m pip install numpy sounddevice
# ffmpeg must also be available in PATH.
Copy-Item .\windows_play_jarvis_wasapi.py "$env:USERPROFILE\VoiceDSP-Jarvis-WASAPI.py"
Copy-Item .\windows_play_jarvis_ogg.ps1 "$env:USERPROFILE\VoiceDSP-Jarvis-Ogg-Test.ps1"
.\windows_run_jarvis_ogg_test.cmd
```

The launcher creates an interactive scheduled task so WASAPI reaches the real
speaker even when the command originates from SSH. The reference recording
contains four "Hey Jarvis" utterances over 12.37 seconds. Compare exactly four
expected utterances with the `DETECTED` lines in the Raspberry Pi journal.

Validated on 2026-07-26 with the Windows Conexant WASAPI output approximately
three metres from the array: master endpoint at 80%, test playback at 50%, five
non-overlapping runs, and exactly 20 detections for 20 recorded utterances.
Wait at least 25 seconds between scheduled-task runs because Windows may take
several seconds to start playback. Overlapping tasks invalidate the result.

A separate five-minute quiet-room soak produced zero false detections. The
highest observed score was `0.008`, versus the `0.30` trigger threshold. This
short check demonstrates the setup, but it does not replace a multi-hour
production false-activation test in varied noise.

Persistence was also checked with a full Raspberry Pi reboot. The SPI firmware
boot service and this detector returned automatically, all profile values read
back correctly, and the first post-reboot `jarvis.ogg` run produced exactly
four detections for four utterances, with no extra event.

This is a demonstration profile, not yet a production wake-word acceptance
test. Production validation still needs multiple voices, distances, angles,
noise conditions, and a multi-hour false-activation run.

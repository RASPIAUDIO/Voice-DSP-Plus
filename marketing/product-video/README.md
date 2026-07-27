# Voice DSP+ 90-second product video

This directory contains the reproducible master used at the top of the
RASPIAUDIO Voice DSP+ product page.

## Storyboard

| Time | Message |
| --- | --- |
| 00:00-00:07 | Product and promise: a complete voice interface for AI products |
| 00:07-00:18 | USB-C plug-and-play workflow and real Web Updater |
| 00:18-00:29 | Four microphones, onboard/external speaker and headphones |
| 00:29-00:39 | On-board beamforming, AEC, NS, AGC, limiter and DoA |
| 00:39-00:48 | Detachable LINE and SQUARE microphone geometries |
| 00:48-00:58 | Raspberry Pi I2S and I2C mode |
| 00:58-01:06 | Product application examples |
| 01:06-01:13 | Real acoustic echo cancellation demo excerpt and audio |
| 01:13-01:20 | Real noise suppression demo excerpt and audio |
| 01:20-01:27 | Real far-field wake-word demo excerpt and audio |
| 01:27-01:30 | RASPIAUDIO close and product URL |

The wake-word scene explicitly states that the pretrained model runs on the
Raspberry Pi. The XMOS XVF3800 is the far-field audio front end and does not run
the machine-learning model in this demo.

## Build

Requirements:

- Python 3.11 or newer;
- NumPy;
- FFmpeg and FFprobe available on `PATH`;
- internet access to download the official RASPIAUDIO source media.

```powershell
python -m pip install numpy
python marketing\product-video\build_voice_dsp_plus_promo.py
```

Outputs:

- `voice-dsp-plus-overview-90s.mp4`: 1920x1080, H.264/AAC, 30 fps;
- `voice-dsp-plus-overview-90s-poster.jpg`: product-page poster;
- `manifest.json`: upstream URLs, SHA256 hashes and final media metadata.

The background music is generated from scratch by the build script. No stock
music, samples or third-party voice-over are used. During the three proof
scenes the music is ducked and the original audio from each full demo is mixed
into the overview. The Web Updater screenshot is stored in `source-assets/` so
the published build remains reproducible even if the live page later changes.

## Product-page integration

The public video is served from:

```text
https://raspiaudio.com/voicedsp/media/voice-dsp-plus-overview-90s.mp4
```

[`product-page-embed.html`](product-page-embed.html) contains the responsive,
non-autoplaying HTML/CSS block placed before the product-description navigation.
The product page keeps the complete AEC, noise-suppression and far-field videos
below this short overview.

# Voice DSP+ 90-second product video

This directory contains the reproducible master used at the top of the
RASPIAUDIO Voice DSP+ product page.

## Storyboard

| Time | Message |
| --- | --- |
| 00:00-00:08 | Voice DSP+ product introduction |
| 00:08-00:18 | USB plug-and-play workflow |
| 00:18-00:27 | Microphone, speaker and headphone I/O |
| 00:27-00:36 | On-board voice DSP features |
| 00:36-00:45 | LINE and SQUARE microphone geometries |
| 00:45-00:54 | Raspberry Pi I2S and I2C mode |
| 00:54-01:02 | Product application examples |
| 01:02-01:10 | Real acoustic echo cancellation demo excerpt |
| 01:10-01:18 | Real noise suppression demo excerpt |
| 01:18-01:26 | Real far-field wake-word demo excerpt |
| 01:26-01:30 | RASPIAUDIO close and website |

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
music, samples or third-party voice-over are used. The three short proof scenes
come from the full validated demos already published on the product page.

## Product-page integration

The public video is served from:

```text
https://raspiaudio.com/voicedsp/media/voice-dsp-plus-overview-90s.mp4
```

[`product-page-embed.html`](product-page-embed.html) contains the responsive,
non-autoplaying HTML/CSS block placed before the product-description navigation.
The product page keeps the complete AEC, noise-suppression and far-field videos
below this short overview.

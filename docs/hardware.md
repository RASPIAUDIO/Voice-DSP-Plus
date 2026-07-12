# Hardware Overview

Voice DSP+ combines an XMOS XVF3800 voice processor with four digital
microphones, a TLV320DAC3101 codec, a PCAL6408A GPIO expander, speaker and
headphone outputs, status LEDs, a user button and a WS2812B ring connection.

## Microphone Modules

The board always uses four microphones. Two mechanical layouts are supported:

- `SQUARE`: better 360-degree direction finding around the board.
- `LINE`: useful for a front-facing product and linear beamforming tests.

The module strap is read from PCAL `P6` once at cold boot:

- `P6=1`: SQUARE.
- `P6=0`: LINE.

Power the board off before changing modules.

## Output Routing

- No jack: mono differential codec path to the onboard speaker amplifier.
- Jack inserted: speaker amplifier muted and stereo single-ended headphone path
  enabled.
- `JACK_DET/P5=1` means a headphone jack is inserted.

## Microphone Order

Top view with USB on the left:

```text
SQUARE                         LINE, left to right

 mic1 -------- mic2            mic0  mic2  mic3  mic1
  |              |
 mic0 -------- mic3
```

## Status Indicators

- Red LED: headphone jack detected.
- Green LED: SQUARE microphone geometry detected.
- Optional 24-pixel ring: processed direction of arrival.

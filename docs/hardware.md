# Hardware Overview

Voice DSP+ combines an XMOS XVF3800 voice processor with four digital
microphones, a TLV320DAC3101 codec, a PCAL6408A GPIO expander, speaker and
headphone outputs, status LEDs, a user button and a WS2812B ring connection.

## Microphone Modules

The board always uses four microphones. Two mechanical layouts are supported:

- `SQUARE`: for voices and sound sources located all around the product.
- `LINE`: for a product whose users are mainly in front of it.

## Choosing LINE or SQUARE

| Choose | Best fit | Typical products | Direction finding |
| --- | --- | --- | --- |
| `SQUARE` | People can speak from any side | Tabletop assistant, conference device, robot, room controller | Best choice for 360-degree azimuth and the NeoPixel direction ring |
| `LINE` | People speak from a known front side | Display, kiosk, soundbar, wall panel | Strong left/right information across the front; front/back ambiguity is inherent to a straight array |

This is a mechanical choice, not an audio-quality setting. Install the module
that matches the physical product. The firmware then loads the matching DSP
geometry automatically.

**Always power Voice DSP+ off before connecting, removing or changing a
microphone module.** The geometry input is sampled only at cold boot. After
power-up, the green status LED confirms that `SQUARE` was detected; green off
means `LINE`.

### SQUARE layout

Use SQUARE when the product sits in the middle of the listening area and a
speaker may approach it from any direction.

![Top view of the SQUARE microphone module](images/microphones-square.svg)

Physical order, top view with USB on the left: clockwise from the upper-left
corner, `D1`, `D2`, `D3`, `D0`.

### LINE layout

Use LINE when the product has a clear front edge. Point that front edge toward
the expected users or voice source.

![Top view of the LINE microphone module](images/microphones-line.svg)

Physical order from left to right, top view with USB on the left: `D0`, `D2`,
`D3`, `D1`.

The module strap is read from PCAL `P6` once at cold boot:

- `P6=1`: SQUARE.
- `P6=0`: LINE.

Power the board off before changing modules.

## Output Routing

- No jack: mono differential codec path to the onboard speaker amplifier.
- Jack inserted: speaker amplifier muted and stereo single-ended headphone path
  enabled.
- `JACK_DET/P5=1` means a headphone jack is inserted.

## Status Indicators

- Red LED: headphone jack detected.
- Green LED: SQUARE microphone geometry detected.
- Optional 24-pixel ring: processed direction of arrival.

See the [official Voice DSP+ product page](https://raspiaudio.com/product/aimic/)
for the complete product presentation.

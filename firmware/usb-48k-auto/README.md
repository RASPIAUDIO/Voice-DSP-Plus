# USB 48 kHz AUTO

This is the recommended Voice DSP+ firmware.

- USB Audio Class, 48 kHz, 2 processed inputs and 2 outputs.
- Automatic LINE/SQUARE microphone geometry selection at cold boot.
- Beamforming, AEC, noise processing, AGC and DOA.
- Automatic mono speaker / stereo headphone routing.
- 24-pixel WS2812B direction ring support.
- User button sends `Ctrl+Shift+D` while held.

The microphone module must be changed only while the board is powered off.
`MIC_DETECT/P6` is sampled during cold boot and remains fixed until restart.

The validated image uses USB PID `20B1:4FC7`. Its current USB descriptor still
appears as `PI AI MIC Assistant Auto`; this legacy string does not change the
Voice DSP+ hardware or feature set.

Use the browser updater at <https://raspiaudio.com/AIMIC/webflasher/> or flash
the DFU image from `artifacts/` with an XMOS-compatible DFU tool.

The `.xe` image is for XTAG/development. The `_spi_boot.bin` image is for a
Raspberry Pi SPI RAM boot. Checksums are in `SHA256SUMS.txt`.

# Raspberry Pi I2S 16 kHz AUTO

This is the recommended Raspberry Pi firmware for Voice DSP+.

- Raspberry Pi is I2S master; XVF3800 is I2S device/slave.
- 16 kHz, 2 processed capture channels and 2 playback channels.
- Automatic LINE/SQUARE microphone geometry selection at cold boot.
- Speaker/headphone routing, jack LED, geometry LED and DOA NeoPixel ring.
- XVF host control over I2C at `0x2c`.
- Persistent SPI boot from the Raspberry Pi, so normal users do not need XTAG.

Install from the repository root with:

```bash
sudo bash raspberrypi/install.sh
sudo reboot
```

After reboot:

```bash
speaker-test -D pulse -c 2 -r 16000 -F S32_LE -t sine -f 1000 -l 1
arecord -D pulse -f S32_LE -r 16000 -c 2 -d 10 ~/voice-dsp-plus.wav
aplay -D pulse ~/voice-dsp-plus.wav
```

The `.xe`, SPI boot image, transfer metadata, runtime scripts and exact source
snapshot used for this validated checkpoint are included in this directory.

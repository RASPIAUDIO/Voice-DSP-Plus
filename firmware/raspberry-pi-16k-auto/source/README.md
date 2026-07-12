# Source snapshot

`xvf3800_source_external_241029_121941/sources/` contains the exact modified
files used for the validated 16 kHz AUTO geometry build.

Overlay these files onto the matching licensed XMOS XVF3800 source release and
build preset `intdev-lr16-auto-rpi-i2c-doa-neopixel`.

The important product change is boot-time geometry selection from PCAL6408A P6:
seven 10 ms samples, six matching samples required, LINE fallback, then a fixed
geometry for the complete boot session.

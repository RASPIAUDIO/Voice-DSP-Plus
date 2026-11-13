# Raspberry Pi host sources

Linux host/controller, SPI RAM loader, desktop integration, installer and tests
for r8. AEC/filter values are unchanged from the tested Pi reference. Paired
images are in [Firmware](../Firmware/README.md); proprietary DSP library sources
are not included here.

## Build without hardware or firmware

On Linux, install `build-essential cmake python3 libasound2 device-tree-compiler`.
From this Sources directory:

```sh
python3 prepare_kit.py --without-firmware --output "$HOME/voicedsp-source-check"
cmake -S "$HOME/voicedsp-source-check" -B "$HOME/voicedsp-source-check/build"
cmake --build "$HOME/voicedsp-source-check/build" -j2
ctest --test-dir "$HOME/voicedsp-source-check/build" --output-on-failure
python3 -m unittest discover -s "$HOME/voicedsp-source-check/tests"
```

Two real-image SPI tests are explicitly skipped without firmware; this is not
a passed hardware test. Source-only assemblies cannot be installed by
`one_command.py`. For the complete suite, use `--firmware-dir PATH` instead,
with the matching images (the default is the adjacent Firmware folder).
Preparation/tests do not play audio.

| Source | Responsibility |
| --- | --- |
| `linux/` | Single-owner controller, local IPC and I2C transport |
| `drivers/` | Portable board/codec/gesture/UI/AEC code |
| `host/v2/pi/` | I2S overlay and clock supervisor |
| `scripts/` | Boot, installation, desktop UI, capture and verification |
| `tests/` | C/C++ and Python tests |
| `tools/` | ALSA configuration generation |
| `accepted-aec.json` | Readback policy, not a live tuning file |
| `prepare_kit.py` | Manifest-checked assembly, no private repository dependency |
| `licenses/` | Third-party notices |

[Installation](QUICKSTART.md) | [Provenance and licensing](UPSTREAM.md).

## Rebuild the downloadable kit

Run from this Sources directory. Use fresh output/archive names:

```sh
python3 prepare_kit.py --output "$HOME/voicedsp-kit-build" --archive "$HOME/voicedsp-kit-build.zip"
python3 tools/build_installer.py --archive "$HOME/voicedsp-kit-build.zip" --output "$HOME/voicedsp-kit-installer.run"
```

Both commands produce SHA256 sidecars. The default paired images are taken from
the adjacent Firmware folder and checked before packaging. The public release
uses distinct ZIP/installer basenames to keep their checksum files separate.

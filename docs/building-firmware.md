# Building Firmware

The full XMOS SDK is not mirrored here. Obtain the licensed
`xvf3800_source_external_241029_121941` release and XMOS tools from XMOS.

Copy `source/xmos-xvf3800-overlay/` over the release `sources/` directory. The
overlay preserves the original relative paths and contains the exact modified
files used by the validated AUTO builds.

USB 48 kHz AUTO preset:

```powershell
powershell -ExecutionPolicy Bypass -File build_xvf3800.ps1 `
  -BuildPreset ua-io48-auto-piaimic-beclear2-speaker-doa-neopixel `
  -ConfigurePreset rel_app_xvf3800_windows
```

Raspberry Pi I2S 16 kHz AUTO preset:

```powershell
powershell -ExecutionPolicy Bypass -File build_xvf3800.ps1 `
  -BuildPreset intdev-lr16-auto-rpi-i2c-doa-neopixel `
  -ConfigurePreset rel_app_xvf3800_windows
```

The current preset and internal symbol names retain `PI_AI_MIC` for binary
compatibility with the validated checkpoints. Product-facing documentation is
now branded Voice DSP+; descriptor and symbol renaming should be done only in a
new hardware-validation cycle.

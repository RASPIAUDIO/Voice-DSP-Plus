# Raspberry Pi 48 kHz release

| Download | Use |
| --- | --- |
| [Installer r8](voicedsp-v2-pi48-r8.run) / [SHA256](voicedsp-v2-pi48-r8.sha256) | Recommended fresh Pi4 installation; includes source, tests and paired XMOS image |
| [Complete kit ZIP](voicedsp-v2-pi48-r8-kit.zip) / [SHA256](voicedsp-v2-pi48-r8-kit.sha256) | Inspect, compile or install manually |
| [SPI RAM image](xmos-i2s48.spi_boot.bin) | Paired I2S image for the supplied loader |
| [XE image](xmos-i2s48.xe) | Matching engineering/debug image; not needed for normal installation |

See [installation](../README.md). This release is for **Voice DSP+ V2 on Pi4,
64-bit Raspberry Pi OS**, not V1 or standalone USB mode. Firmware loads into
RAM automatically each boot; the normal installer does not write QSPI or use XTAG.

| Paired image | SHA256 |
| --- | --- |
| `xmos-i2s48.spi_boot.bin` (864256 bytes) | `152334171334c9608783985ee83ed4e3daad94385dd220bc4a5884b4eaa7dc50` |
| `xmos-i2s48.xe` | `ee772562ce81b87e535d09175a6f5ed8cd15655983843b03af4e2bb331093d12` |

Use the host and firmware together. Do not substitute a USB image or the older
main-branch Pi16 image. [Source/build guide](../Sources/README.md) |
[Release scope](../Documentation/release-notes.md) |
[Notices](../../NOTICE.md).

# Switch an existing USB V2 to host-controlled I2S

[Open the I2S installer](https://raspiaudio.com/voicedsp/webflasher/?firmware=v2-i2s-48k)

Connect the XMOS USB data port of a V2 already running USB firmware. Stop
playback and select **V2 - I2S 48 kHz - Host-controlled (UI2)**. This downloads
an upgrade image, not a full factory image and not an ESP32 image.

**After reboot, USB audio and browser updating disappear.** This is expected:
the host must configure the I2S application. To return to USB, use
[ZeroCore S3 USB Recovery](https://apps.raspiaudio.com/#device=ZeroCoreS3&application=USBRecovery)
then the USB updater, or provision through Raspberry Pi/SPI. A factory RAM
tester without DFU cannot install this file directly.

For Home Assistant, normally use its [ESP32 installer](https://apps.raspiaudio.com/#device=ZeroCoreS3&application=VoiceDSPV2)
instead: it already carries the matching XMOS application. For Raspberry Pi,
use the complete [qualified kit](../../Raspberry%20Pi/README.md). Its previously
qualified XMOS image differs in UI/LED commands; this package does not replace
the Pi kit or establish new Pi compatibility results.

## Provenance and verification

- Application: accepted HA I2S 48 kHz, application version 0.6.0, UI protocol 2.
- Source XE SHA256: `53b45056dd10988e171b439b9cdc418d3d6876dd3f72ec6a6c9c203e80c6f564`.
- Upgrade: `voicedsp-v2-i2s48-ui2.dfu_upgrade.bin`, 861184 bytes, SHA256 in `SHA256SUMS`.
- No application recompilation, AEC change, EQ/filter change or new acoustic claim.
- USB transfer, confirmation/cancel, hashes and catalog routing tested in software.
- **The new USB-to-I2S installation path has not yet been validated on hardware.**

Generated offline with XMOS XTC 15.3.1:

```text
xflash --noinq --boot-partition-size 2097152 --factory-version 15.3 --upgrade 1 voicedsp-controls.xe -o voicedsp-v2-i2s48-ui2.dfu_upgrade.bin
```

No XTAG is used to generate this file. Distribution uses the existing XMOS
package terms included with the Home Assistant firmware.

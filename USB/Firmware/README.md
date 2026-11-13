# USB 48 kHz mono: accepted 0.8.9-r4

[Download USB update](voicedsp-v2-usb-mono-0.8.9.dfu_upgrade.bin) |
[Checksums](SHA256SUMS) | [Release record](release.json)

**Voice DSP+ V2 only.** Not V1, not ESP32, not Raspberry Pi I2S. These are the
original accepted binaries; no acoustic parameters were changed for publication.

## Existing USB board

[Open the V2 web updater](https://raspiaudio.com/voicedsp/webflasher/?firmware=v2-usb-mono-48k)
in desktop Chrome/Edge. **0.8.9-r4** is preselected. Stop all audio and control
apps, connect the XMOS USB data port, confirm the hardware and click Install.
The updater checks size/SHA256 and refuses V1/V2 cross-flashing. It temporarily
mutes V2 without replacing the saved output, level or crossover settings.
Check the output and volume before resuming playback after reboot.

Stop playback first. The board must already enumerate with compatible V2 USB
DFU firmware. Use the **upgrade** on **interface 4 / alternate 1**, never the
factory file. This preserves the factory fallback and settings area.

With [dfu-util](https://dfu-util.sourceforge.net/), list devices:

```sh
dfu-util -l
```

Replace `CURRENT_SERIAL` with the serial in that listing, then run:

```sh
dfu-util -d 20b1:4fc7 -S CURRENT_SERIAL -i 4 -a 1 -D voicedsp-v2-usb-mono-0.8.9.dfu_upgrade.bin
dfu-util -d 20b1:4fc7 -S CURRENT_SERIAL -i 4 -a 1 -e
```

Keep power connected until completion. Check the output and volume before playing.
Expected release `0089`, serial `VDSP2-USBMONO-089-DEV`.

The V1 choices remain in a separate group. Do not select them for a V2 board.
The [web updater source](../WebInstaller/README.md) is versioned alongside this release.

## Blank board / recovery

A blank XMOS or I2S-only firmware cannot receive USB DFU until a USB boot image
is running. Initial provisioning needs a compatible ZeroCore SPI loader or XTAG.
The ordinary Home Assistant image is **not** a USB provisioner.

| File | Purpose |
| --- | --- |
| [`.dfu_upgrade.bin`](voicedsp-v2-usb-mono-0.8.9.dfu_upgrade.bin) | Normal USB update, interface 4 / alternate 1 |
| [`.factory.bin`](voicedsp-v2-usb-mono-0.8.9.factory.bin) | Complete QSPI factory image for a compatible programmer, not USB DFU |
| [`.spi_boot.bin`](voicedsp-v2-usb-mono-0.8.9.spi_boot.bin) | Temporary SPI RAM load; not persistent by itself |
| [`.xe`](voicedsp-v2-usb-mono-0.8.9.xe) | XMOS tools / engineering recovery |

These are generated firmware images, not a user's flash dump. Factory recovery
may erase settings. The r4 checkpoint tested cold boot and subsequent Windows
volume/mute controls on both outputs, not long-term endurance or maximum power.

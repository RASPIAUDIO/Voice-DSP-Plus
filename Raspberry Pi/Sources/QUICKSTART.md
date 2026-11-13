# Install the complete Pi4 kit

This folder contains source, tests and the paired firmware in `firmware/` when
assembled as a complete kit. If it was assembled with `--without-firmware`, it
is for offline source review only and installation is refused.

Use a fresh 64-bit Raspberry Pi OS on Pi4 Model B, Voice DSP+ V2 on the header,
no ZeroCore attached. External / Full Speaker require a USB-C PD charger directly
on Voice DSP+ (65 W or higher recommended). The board powers the Pi via the header.
Power off before rewiring; disconnect the Pi USB-C supply. Never power both USB-C
inputs simultaneously.

From this directory, as the normal Pi user:

```sh
sudo python3 scripts/one_command.py --confirm-v2-exclusive --desktop --reboot
```

This installs dependencies, compiles/tests locally and reboots. No QSPI flashing
or XTAG is required. After reboot:

```sh
journalctl -u voicedsp-v2-48k-setup.service -b --no-pager
python3 /opt/voicedsp-v2-48k/scripts/verify_installation.py
```

Desktop speaker/microphone default to Voice DSP+. Use the panel icons for
volume/mute and Voice DSP+ Output for Internal / External / Full Speaker.
Full Speaker is required for the matched L woofer / R tweeter assembly;
unfiltered External is only for full-range speakers or passive crossovers.

The core starts OFF/-30 dB; desktop setup selects Internal at hardware 0 dB,
initially 25% desktop volume. The desktop volume is remembered, not the route.

For a desktop-only update of an existing core, stop playback first:

```sh
sudo python3 scripts/desktop.py install --user "$USER"
```

Relaunch Voice DSP+ Output after updating. This does not reflash XMOS but briefly
restarts user audio services. For headless installations omit `--desktop`.

To uninstall, remove desktop integration first, then stop and remove the core:

```sh
sudo python3 /opt/voicedsp-desktop/desktop.py remove
sudo systemctl stop voicedsp-v2-48k.service
sudo python3 /opt/voicedsp-v2-48k/scripts/install.py uninstall --execute --confirm-v2-exclusive
```

User-modified managed files are preserved rather than deleted. Reboot after
removing the overlay. Full guide and diagnostics:
[Raspberry Pi documentation](https://github.com/RASPIAUDIO/Voice-DSP-Plus/tree/V2/Raspberry%20Pi).

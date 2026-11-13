# USB sources

Source package for the accepted **0.8.9-r4**, not earlier 0.8.9 candidates.
[Ready-to-install images](../Firmware/README.md) | [Provenance](../../UPSTREAM.md)

## What is included

- `XMOS/`: integration files and exact edits against the licensed XVF3800 3.2.1 SDK.
- `overlay/`, `include/`, `src/`, `tests/`: standalone USB state, volume, settings,
  buttons and filter tests. The authoritative firmware source is in `XMOS/`.
- `tools/usb_mono_control.py`: optional USB control client (Python 3 + PyUSB/libusb).
- `prepare_xmos.py`: reconstructs the source with verified hashes, without
  accessing hardware or modifying the input SDK.

The DSP SDK itself is not mirrored. Supply your original
`xvf3800_source_external_241029_121941.tar`; its required SHA256 is recorded in
`XMOS/changes.json`. Our source changes do not replace the licensed DSP libraries.

## Reconstruct and build

Use XMOS XTC **15.3.1**, Python **3.10**, CMake/Ninja and the Python dependencies
specified by that SDK. Run in an XTC command environment. Use short local paths
on Windows to avoid build path-length limits.

```sh
python prepare_xmos.py --sdk /path/to/xvf3800_source_external_241029_121941.tar --output /path/to/new-usb-tree
```

Then, from the reconstructed `sources/` directory on Windows:

```powershell
python -m pip install -r requirements_build.txt
$modules = (Resolve-Path modules/fwk_xvf/modules).Path
$env:PYTHONPATH = "$modules/yaml_autogen/src;$modules/eq_filter_tuning;$modules/nlmodel_tuning;$modules/tuning"
cmake --preset rel_app_xvf3800_windows -DVOICEDSP_V2_CAPTURE=ON -DPython3_EXECUTABLE=/path/to/python.exe
cmake --build build/app_xvf3800 --target application_xvf3800_v2-usb-mono-ua-io48-auto-piaimic-beclear2-speaker-doa-neopixel
```

This produces an XE, not a flash operation. The release download contains the
already accepted factory, SPI and DFU images. Rebuilding with different tools
does not imply byte-identical images or a new acoustic validation.

To generate new QSPI/DFU files offline from your rebuilt XE (XTC 15.3.1):

```sh
xflash --noinq --factory YOUR_BUILD.xe --boot-partition-size 0x200000 --quad-spi-clock 5MHz -o rebuilt.factory.bin
xflash --noinq --upgrade 1 YOUR_BUILD.xe --factory-version 15.3 --quad-spi-clock 5MHz -o rebuilt.dfu_upgrade.bin
```

Keep both `--noinq` and `-o`: these commands create files, not a device flash.

## Offline checks

```sh
python -m unittest discover -s tools -p "test_*.py"
python tools/run_native_tests.py --output /path/to/new-test-directory
python tools/verify_release.py
```

Native tests require a C11 compiler (`cc`, or `cl` from a Visual Studio developer
shell). None of these commands scans, flashes or plays audio on a device.

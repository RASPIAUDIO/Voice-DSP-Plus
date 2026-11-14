# Home Assistant sources

Source package for **ESPHome 0.6.68**, including the owner-validated PAJ recovery.
[Install the original binary](../Firmware/README.md) | [Provenance](../../UPSTREAM.md)

## ESP32

`ESPHome/zerocore-v2-web.yaml` is the entry point. Local packages, components,
models, UI sounds, dependency lock and the exact embedded XMOS image are included.
The developer's absolute build directory was removed and the local Improv path
in the reference lock file was made relative; firmware logic is unchanged.
Wi-Fi is provisioned after installation; no personal credentials are included.

Use Python 3.12 with **ESPHome 2026.8.1** (ESP-IDF 5.5.5 in the dependency lock). In a virtual
environment, from `ESPHome/`:

```sh
python -m pip install esphome==2026.8.1
python -m esphome config zerocore-v2-web.yaml
python -m esphome compile zerocore-v2-web.yaml
```

Internet is needed for ESPHome/ESP-IDF dependencies on a fresh computer. Keep
`dependencies.lock` for comparison with the accepted build. The unchanged
factory/OTA downloads remain the release reference; a local rebuild is not
automatically an acoustically validated release.

## Matching XMOS source

`XMOS/` contains the source changes for the exact I2S/UI image embedded in ESP32,
including the 0.6.67 LED controls. Version 0.6.68 changes only the ESP32 PAJ driver.
Use the shared [reconstruction tool](../../USB/Sources/prepare_xmos.py) with your
licensed XVF3800 3.2.1 archive:

```sh
python "../../USB/Sources/prepare_xmos.py" --sdk /path/to/xvf3800_source_external_241029_121941.tar --delta XMOS --output /path/to/new-ha-xmos-tree
```

In the reconstructed `sources/`, using the XTC 15.3.1 environment, Python 3.10
and SDK Python dependencies:

```powershell
python -m pip install -r requirements_build.txt
$modules = (Resolve-Path modules/fwk_xvf/modules).Path
$env:PYTHONPATH = "$modules/yaml_autogen/src;$modules/eq_filter_tuning;$modules/nlmodel_tuning;$modules/tuning"
cmake --preset rel_app_xvf3800_windows -DVOICEDSP_V2_CAPTURE=ON -DPython3_EXECUTABLE=/path/to/python.exe
cmake --build build/app_xvf3800 --target application_xvf3800_v2-i2s-ui-intdev-lr48-auto-rpi-i2c-doa-neopixel
```

The ESP32 sends its embedded SPI image to XMOS RAM on every boot. This version
does not install a persistent XMOS QSPI image. The USB release has a different
transport and cannot be substituted here.

Do not replace AEC profiles or crossover defaults with another mode's values.
This package deliberately preserves the previously accepted settings.

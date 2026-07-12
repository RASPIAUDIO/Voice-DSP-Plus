#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_DIR="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
ARTIFACT_DIR="$PACKAGE_DIR/bin"
IMAGE="$ARTIFACT_DIR/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_2026-07-10_spi_boot.bin"
METADATA="$ARTIFACT_DIR/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_2026-07-10_spi_boot.json"
SERVICE="$PACKAGE_DIR/raspberrypi/systemd/pi-ai-mic-rpi-16k-auto-spi-boot.service"

for path in "$IMAGE" "$METADATA" "$SERVICE" "$SCRIPT_DIR/pi-ai-mic-rpi-spi-boot.py"; do
    if [ ! -f "$path" ]; then
        echo "Missing package file: $path" >&2
        exit 2
    fi
done

sudo install -d -m 0755 /opt/pi-ai-mic/firmware
sudo install -m 0644 "$IMAGE" /opt/pi-ai-mic/firmware/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_spi_boot.bin
sudo install -m 0644 "$METADATA" /opt/pi-ai-mic/firmware/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_spi_boot.json
sudo install -m 0755 "$SCRIPT_DIR/pi-ai-mic-rpi-spi-boot.py" /usr/local/bin/pi-ai-mic-rpi-spi-boot.py
sudo install -m 0755 "$SCRIPT_DIR/pi-ai-mic-rpi-16k-auto-spi-autoboot.sh" /usr/local/bin/pi-ai-mic-rpi-16k-auto-spi-autoboot.sh
sudo install -m 0644 "$SERVICE" /etc/systemd/system/pi-ai-mic-rpi-16k-auto-spi-boot.service

for old_service in \
    pi-ai-mic-rpi-48k-auto-spi-boot.service \
    pi-ai-mic-rpi-v1-1-doa-spi-boot.service \
    pi-ai-mic-rpi-16k-line-doa-spi-boot.service \
    pi-ai-mic-rpi-48k-doa-spi-boot.service; do
    sudo systemctl disable --now "$old_service" >/dev/null 2>&1 || true
done

sudo systemctl daemon-reload
sudo systemctl enable --now pi-ai-mic-rpi-16k-auto-spi-boot.service
sudo systemctl --no-pager --full status pi-ai-mic-rpi-16k-auto-spi-boot.service

echo
echo "Installed. Geometry is sampled from PCAL P6 only when the XVF3800 boots."
echo "Power the Pi off before changing between LINE and SQUARE, then cold-boot it."

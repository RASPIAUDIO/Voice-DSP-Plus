#!/usr/bin/env bash
set -euo pipefail

PACKAGE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TARGET_USER="${TARGET_USER:-}"
FIRMWARE_DIR="${PI_AI_MIC_FIRMWARE_DIR:-/opt/pi-ai-mic/firmware}"

if [ -z "$TARGET_USER" ] || [ "$TARGET_USER" = "root" ]; then
  TARGET_USER="${SUDO_USER:-${USER:-}}"
fi
if { [ -z "$TARGET_USER" ] || [ "$TARGET_USER" = "root" ]; } && command -v logname >/dev/null 2>&1; then
  TARGET_USER="$(logname 2>/dev/null || printf '%s' "$TARGET_USER")"
fi
if [ -z "$TARGET_USER" ] || [ "$TARGET_USER" = "root" ]; then
  echo "Cannot determine non-root target user; set TARGET_USER or use the product installer." >&2
  exit 2
fi
if ! getent passwd "$TARGET_USER" >/dev/null; then
  echo "Target user does not exist: $TARGET_USER" >&2
  exit 1
fi

TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
if [ -z "$TARGET_HOME" ]; then
  echo "Cannot determine home directory for target user: $TARGET_USER" >&2
  exit 1
fi

install -d "$FIRMWARE_DIR"
install -m 0644 \
  "$PACKAGE_ROOT/bin/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_2026-07-10_spi_boot.bin" \
  "$FIRMWARE_DIR/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_spi_boot.bin"
install -m 0644 \
  "$PACKAGE_ROOT/bin/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_2026-07-10_spi_boot.json" \
  "$FIRMWARE_DIR/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_spi_boot.json"

install -m 0755 "$PACKAGE_ROOT/raspberrypi/scripts/pi-ai-mic-rpi-spi-boot.py" \
  /usr/local/bin/pi-ai-mic-rpi-spi-boot.py
install -m 0755 "$PACKAGE_ROOT/raspberrypi/scripts/pi-ai-mic-rpi-16k-auto-spi-autoboot.sh" \
  /usr/local/bin/pi-ai-mic-rpi-16k-auto-spi-autoboot.sh
install -m 0755 "$PACKAGE_ROOT/raspberrypi/scripts/pi-ai-mic-vocalfusion-pipewire-16k-user.sh" \
  /usr/local/bin/pi-ai-mic-vocalfusion-pipewire-16k-user.sh

install -m 0644 "$PACKAGE_ROOT/raspberrypi/systemd/pi-ai-mic-rpi-16k-auto-spi-boot.service" \
  /etc/systemd/system/pi-ai-mic-rpi-16k-auto-spi-boot.service

install -d "$TARGET_HOME/.config/systemd/user"
install -m 0644 "$PACKAGE_ROOT/raspberrypi/systemd/pi-ai-mic-vocalfusion-pipewire-16k.service" \
  "$TARGET_HOME/.config/systemd/user/pi-ai-mic-vocalfusion-pipewire-16k.service"
chown -R "$TARGET_USER:" "$TARGET_HOME/.config/systemd"

for service in \
  pi-ai-mic-fwowned-spi-boot.service \
  pi-ai-mic-rpi-v1-1-doa-spi-boot.service \
  pi-ai-mic-rpi-48k-auto-spi-boot.service \
  pi-ai-mic-rpi-16k-line-doa-spi-boot.service \
  pi-ai-mic-rpi-48k-doa-spi-boot.service; do
  systemctl disable --now "$service" 2>/dev/null || true
done
systemctl daemon-reload
systemctl enable pi-ai-mic-rpi-16k-auto-spi-boot.service

loginctl enable-linger "$TARGET_USER" 2>/dev/null || true
sudo -u "$TARGET_USER" XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
  systemctl --user daemon-reload || true
sudo -u "$TARGET_USER" XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
  systemctl --user disable --now pi-ai-mic-vocalfusion-pipewire-48k.service 2>/dev/null || true
sudo -u "$TARGET_USER" XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
  systemctl --user enable pi-ai-mic-vocalfusion-pipewire-16k.service || true

echo "Installed PI_AI_MIC Raspberry Pi I2S 16 kHz AUTO geometry."
echo "Cold-boot after changing microphone geometry."

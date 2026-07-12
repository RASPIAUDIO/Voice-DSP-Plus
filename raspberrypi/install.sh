#!/usr/bin/env bash
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "Run with: sudo bash raspberrypi/install.sh" >&2
    exit 1
fi

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
PACKAGE_DIR="$REPO_ROOT/firmware/raspberry-pi-16k-auto"
TARGET_USER="${TARGET_USER:-${SUDO_USER:-}}"

if [ -z "$TARGET_USER" ] || [ "$TARGET_USER" = root ]; then
    TARGET_USER="$(logname 2>/dev/null || true)"
fi
if [ -z "$TARGET_USER" ] || [ "$TARGET_USER" = root ]; then
    echo "Cannot determine desktop user. Run with TARGET_USER=<user>." >&2
    exit 2
fi

boot_dir() {
    if [ -d /boot/firmware/overlays ]; then
        printf '%s\n' /boot/firmware
    else
        printf '%s\n' /boot
    fi
}

ensure_line() {
    local file="$1"
    local line="$2"
    grep -qxF "$line" "$file" || printf '\n%s\n' "$line" >> "$file"
}

echo "Installing RASPIAUDIO Voice DSP+ Raspberry Pi 16 kHz AUTO mode"
echo "Target desktop user: $TARGET_USER"

apt-get update
apt-get install -y \
    alsa-utils device-tree-compiler i2c-tools libasound2-plugins \
    pipewire pipewire-alsa pipewire-pulse pulseaudio-utils \
    python3-smbus python3-spidev wireplumber

BOOT_DIR="$(boot_dir)"
CONFIG_TXT="$BOOT_DIR/config.txt"
OVERLAY_DST="$BOOT_DIR/overlays/voice-dsp-plus-xvf3800.dtbo"

install -d "$BOOT_DIR/overlays"
dtc -@ -I dts -O dtb -o "$OVERLAY_DST" \
    "$SCRIPT_DIR/overlay/pi-ai-mic-xvf3800-overlay.dts"

[ -f "$CONFIG_TXT" ] && cp "$CONFIG_TXT" "$CONFIG_TXT.voice-dsp-plus.bak.$(date +%Y%m%d-%H%M%S)"
touch "$CONFIG_TXT"
ensure_line "$CONFIG_TXT" "dtparam=i2c_arm=on"
ensure_line "$CONFIG_TXT" "dtparam=i2s=on"
ensure_line "$CONFIG_TXT" "dtparam=spi=on"

HAT_PRODUCT=""
if [ -f /proc/device-tree/hat/product ]; then
    HAT_PRODUCT="$(tr -d '\000' < /proc/device-tree/hat/product)"
fi
case "$HAT_PRODUCT" in
    "PI AI MIC XVF3800 HAT"|"RASPIAUDIO Voice DSP+ HAT")
        echo "HAT EEPROM detected: $HAT_PRODUCT"
        ;;
    *)
        # The DTS filename differs from the installed dtbo name, so use the
        # installed product name explicitly for unprogrammed prototypes.
        ensure_line "$CONFIG_TXT" "dtoverlay=voice-dsp-plus-xvf3800"
        echo "No supported HAT EEPROM detected; enabled manual overlay."
        ;;
esac

install -d /usr/share/alsa/ucm2/HAT
install -m 0644 "$SCRIPT_DIR/ucm2/HAT/HAT.conf" /usr/share/alsa/ucm2/HAT/HAT.conf
install -m 0644 "$SCRIPT_DIR/ucm2/HAT/HiFi.conf" /usr/share/alsa/ucm2/HAT/HiFi.conf
install -d /opt/voice-dsp-plus/host-control
install -m 0644 "$SCRIPT_DIR/host-control/transport_config.yaml" \
    /opt/voice-dsp-plus/host-control/transport_config.yaml

TARGET_USER="$TARGET_USER" \
    bash "$PACKAGE_DIR/raspberrypi/scripts/install-16k-auto-doa-neopixel.sh"

for group in audio i2c spi; do
    getent group "$group" >/dev/null && usermod -a -G "$group" "$TARGET_USER" || true
done

install -d /opt/voice-dsp-plus
cat > /opt/voice-dsp-plus/product.env <<EOF
VOICE_DSP_PLUS_MODE=16k-auto
VOICE_DSP_PLUS_RATE=16000
VOICE_DSP_PLUS_TARGET_USER=$TARGET_USER
VOICE_DSP_PLUS_LEGACY_RUNTIME_PREFIX=pi-ai-mic
EOF

echo
echo "Installation complete. Reboot with: sudo reboot"
echo "Then test playback and capture with the commands in README.md."

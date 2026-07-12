#!/usr/bin/env bash
set -euo pipefail

PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH"

FIRMWARE_DIR="${PI_AI_MIC_FIRMWARE_DIR:-/opt/pi-ai-mic/firmware}"
SPI_BOOT_SCRIPT="${PI_AI_MIC_SPI_BOOT_SCRIPT:-/usr/local/bin/pi-ai-mic-rpi-spi-boot.py}"
SPI_BOOT_IMAGE="${PI_AI_MIC_SPI_BOOT_IMAGE:-$FIRMWARE_DIR/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_spi_boot.bin}"
SPI_BOOT_JSON="${PI_AI_MIC_SPI_BOOT_JSON:-$FIRMWARE_DIR/pi_ai_mic_rpi_i2s_2ch_16khz_auto_geometry_spi_boot.json}"
SPI_SPEED_HZ="${PI_AI_MIC_SPI_SPEED_HZ:-5000000}"

wait_for_path() {
    local path="$1"
    local label="$2"

    for _ in $(seq 1 50); do
        [ -e "$path" ] && return 0
        sleep 0.1
    done

    echo "$label not available: $path" >&2
    return 1
}

wait_for_pcal() {
    for _ in $(seq 1 50); do
        if i2cget -y 1 0x20 0x03 >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
    done

    echo "PCAL6408A not reachable on I2C bus 1 address 0x20" >&2
    return 1
}

read_transfer_block_num() {
    python3 - "$SPI_BOOT_JSON" <<'PY'
import json
import sys
from pathlib import Path

meta = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
print(meta["transfer_block_num"])
PY
}

main() {
    for path in "$SPI_BOOT_SCRIPT" "$SPI_BOOT_IMAGE" "$SPI_BOOT_JSON"; do
        if [ ! -f "$path" ]; then
            echo "Missing required file: $path" >&2
            exit 2
        fi
    done

    modprobe i2c-dev 2>/dev/null || true
    modprobe spi-bcm2835 2>/dev/null || modprobe spi_bcm2835 2>/dev/null || true
    modprobe spidev 2>/dev/null || true

    wait_for_path /dev/i2c-1 "I2C bus"
    wait_for_path /dev/spidev0.0 "SPI device"
    wait_for_pcal

    local transfer_block_num
    transfer_block_num="$(read_transfer_block_num)"

    echo "Booting PI AI MIC I2S 16 kHz AUTO geometry over SPI"
    echo "Image: $SPI_BOOT_IMAGE"
    echo "transfer_block_num=$transfer_block_num speed=$SPI_SPEED_HZ"

    python3 "$SPI_BOOT_SCRIPT" "$SPI_BOOT_IMAGE" \
        --transfer-block-num "$transfer_block_num" \
        --transfer-delay-ms 5 \
        --spi-speed-hz "$SPI_SPEED_HZ" \
        --reset-release hiz \
        --boot-sel-release hiz \
        --reload-spi-drivers
}

main "$@"

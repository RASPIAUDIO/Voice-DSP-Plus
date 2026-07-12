#!/usr/bin/env bash
set -euo pipefail
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH"

VF_DIR="${VF_DIR:-}"
if [ -n "${BOARD_INIT:-}" ]; then
    BOARD_INIT="$BOARD_INIT"
elif [ -n "$VF_DIR" ]; then
    BOARD_INIT="$VF_DIR/resources/clk_dac_setup/setup_io_exp_and_dac.py"
else
    BOARD_INIT=""
fi
HOST_CONTROL_DIR="${HOST_CONTROL_DIR:-/opt/pi-ai-mic/host_control}"
XVF_HOST="${XVF_HOST:-$HOST_CONTROL_DIR/xvf_host}"
SINK_NAME="${SINK_NAME:-pi_ai_mic_vocalfusion_speaker}"
SOURCE_NAME="${SOURCE_NAME:-pi_ai_mic_vocalfusion_microphone}"
SPEAKER_VOLUME="${SPEAKER_VOLUME:-33%}"
POLL_INTERVAL="${POLL_INTERVAL:-1}"
LED_INDICATORS="${LED_INDICATORS:-1}"
ACTIVE_SINK_NAME="$SINK_NAME"

PCAL=0x20
PCAL_INPUT=0x00
PCAL_OUTPUT=0x01
PCAL_CONFIG=0x03
P5_JACK_MASK=0x20
P6_MIC_MASK=0x40
OUT_HEADPHONE=0x05
OUT_SPEAKER=0x85
CONFIG_PI_AI_MIC=0x62

wait_for_pactl() {
    for _ in $(seq 1 30); do
        pactl info >/dev/null 2>&1 && return 0
        sleep 1
    done
    echo "PipeWire/Pulse is unavailable" >&2
    return 1
}

find_xmos_card_number() {
    awk '/XMOSDevice|PI AI MIC XVF3800 HAT|PIAIMIC|radioi2soutput|radio_i2s_output/ {gsub(/^ +|:$/, "", $1); print $1; exit}' /proc/asound/cards
}

wait_for_xmos_card() {
    local card
    for _ in $(seq 1 30); do
        card="$(find_xmos_card_number)"
        if [ -n "$card" ]; then
            printf '%s\n' "$card"
            return 0
        fi
        sleep 1
    done
    echo "XMOS/PI_AI_MIC ALSA card not found" >&2
    return 1
}
find_alsa_device_number() {
    local list_cmd="$1"
    local card="$2"

    command -v "$list_cmd" >/dev/null 2>&1 || return 0
    "$list_cmd" -l 2>/dev/null | awk -v card="$card" '
        $0 ~ ("card " card ":") {
            for (i = 1; i <= NF; i++) {
                if ($i == "device") {
                    gsub(":", "", $(i + 1))
                    print $(i + 1)
                    exit
                }
            }
        }
    '
}

unload_old_nodes() {
    pactl list short modules 2>/dev/null | awk -F '\t' '
        $2 ~ /^module-alsa-(sink|source)$/ &&
        ($0 ~ /pi_ai_mic_vocalfusion_/ || $0 ~ /xmos_hw_/ || $0 ~ /device=hw:[0-9]+,[01]/) { print $1 }
    ' | while read -r id; do
        [ -n "$id" ] && pactl unload-module "$id" >/dev/null 2>&1 || true
    done
}

load_pipewire_nodes() {
    local card="$1"
    local auto_sink=""
    local source_result=""
    local playback_dev=""
    local capture_dev=""
    playback_dev="$(find_alsa_device_number aplay "$card")"
    capture_dev="$(find_alsa_device_number arecord "$card")"
    playback_dev="${playback_dev:-0}"
    capture_dev="${capture_dev:-1}"
    echo "ALSA devices: card=$card playback=$playback_dev capture=$capture_dev"

    unload_old_nodes

    if pactl load-module module-alsa-sink device="hw:${card},${playback_dev}" sink_name="$SINK_NAME" rate=16000 channels=2 format=s32le >/dev/null; then
        ACTIVE_SINK_NAME="$SINK_NAME"
    else
        auto_sink="$(pactl list short sinks 2>/dev/null | awk -F '\t' '/alsa_output.*soc_sound/ { print $2; exit }')"
        if [ -z "$auto_sink" ]; then
            echo "Unable to create PI_AI_MIC PipeWire sink and no auto sink exists" >&2
            return 1
        fi
        ACTIVE_SINK_NAME="$auto_sink"
        echo "Using existing PipeWire sink: $ACTIVE_SINK_NAME"
    fi

    if source_result="$(pactl load-module module-alsa-source device="hw:${card},${capture_dev}" source_name="$SOURCE_NAME" rate=16000 channels=2 format=s32le 2>&1)"; then
        if pactl list short sources | awk -F '\t' -v name="$SOURCE_NAME" '$2 == name { found = 1 } END { exit found ? 0 : 1 }'; then
            pactl set-default-source "$SOURCE_NAME" >/dev/null 2>&1 || true
        else
            echo "PI_AI_MIC source module loaded but source '$SOURCE_NAME' is missing; module=$source_result" >&2
            return 1
        fi
    else
        echo "PI_AI_MIC source module not loaded; pactl said: $source_result" >&2
        return 1
    fi

    pactl set-default-sink "$ACTIVE_SINK_NAME" >/dev/null 2>&1 || true
    pactl set-sink-volume "$ACTIVE_SINK_NAME" "$SPEAKER_VOLUME" >/dev/null 2>&1 || true
    echo "PipeWire nodes ready: sink=$ACTIVE_SINK_NAME source=$SOURCE_NAME card=$card playback=$playback_dev capture=$capture_dev volume=$SPEAKER_VOLUME"
}

run_board_init() {
    if [ -n "$BOARD_INIT" ] && [ -f "$BOARD_INIT" ]; then
        python3 "$BOARD_INIT" xvf3800-intdev || true
    fi
}

run_xvf_i2c() {
    [ -x "$XVF_HOST" ] || return 0
    (cd "$HOST_CONTROL_DIR" && ./xvf_host --use i2c "$@") >/dev/null 2>&1 || true
}

configure_xvf_minimal() {
    run_xvf_i2c I2S_DAC_DSP_ENABLE 1
    echo "XVF minimal route applied: I2S_DAC_DSP_ENABLE=1"
}

read_pcal_input() {
    i2cget -y 1 "$PCAL" "$PCAL_INPUT" b 2>/dev/null || true
}

configure_pcal_output() {
    local out="$1"
    i2cset -y 1 "$PCAL" "$PCAL_OUTPUT" "$out" b >/dev/null 2>&1 || true
    i2cset -y 1 "$PCAL" "$PCAL_CONFIG" "$CONFIG_PI_AI_MIC" b >/dev/null 2>&1 || true
}

configure_dac_speaker_diff_muted() {
    # Restore the full 16 kHz DAC3101 init before selecting the speaker route.
    # A route-only restore gives valid I2S activity but can leave the analog
    # output silent after a cold boot or DAC reset.
    i2cset -y 1 0x18 0x00 0x00 b
    i2cset -y 1 0x18 0x01 0x01 b
    sleep 0.1
    i2cset -y 1 0x18 0x06 0x18 b
    i2cset -y 1 0x18 0x08 0x00 b
    i2cset -y 1 0x18 0x07 0x00 b
    i2cset -y 1 0x18 0x1e 0x81 b
    sleep 0.1
    i2cset -y 1 0x18 0x04 0x07 b
    i2cset -y 1 0x18 0x05 0x94 b
    i2cset -y 1 0x18 0x0b 0x84 b
    i2cset -y 1 0x18 0x0c 0x86 b
    i2cset -y 1 0x18 0x0e 0x00 b
    i2cset -y 1 0x18 0x0d 0x01 b
    i2cset -y 1 0x18 0x19 0x04 b
    i2cset -y 1 0x18 0x1a 0x81 b
    i2cset -y 1 0x18 0x33 0x10 b
    i2cset -y 1 0x18 0x1b 0x20 b
    i2cset -y 1 0x18 0x3c 0x01 b

    i2cset -y 1 0x18 0x00 0x01 b
    i2cset -y 1 0x18 0x1f 0x14 b
    i2cset -y 1 0x18 0x21 0x4e b
    i2cset -y 1 0x18 0x23 0x41 b
    i2cset -y 1 0x18 0x24 0x80 b
    i2cset -y 1 0x18 0x25 0x80 b
    i2cset -y 1 0x18 0x26 0xa8 b
    i2cset -y 1 0x18 0x27 0xa8 b
    i2cset -y 1 0x18 0x28 0x06 b
    i2cset -y 1 0x18 0x29 0x06 b
    i2cset -y 1 0x18 0x2a 0x04 b
    i2cset -y 1 0x18 0x2b 0x04 b
    i2cset -y 1 0x18 0x1f 0xd4 b
    i2cset -y 1 0x18 0x20 0xc6 b

    i2cset -y 1 0x18 0x00 0x00 b
    i2cset -y 1 0x18 0x40 0x0c b
    i2cset -y 1 0x18 0x41 0x00 b
    i2cset -y 1 0x18 0x42 0x00 b
    i2cset -y 1 0x18 0x3f 0x90 b
    echo "PI_AI_MIC DAC route: full 16 kHz speaker differential init, muted"
}

configure_dac_headphone() {
    i2cset -y 1 0x18 0x00 0x01 b
    i2cset -y 1 0x18 0x23 0x44 b
    i2cset -y 1 0x18 0x24 0x80 b
    i2cset -y 1 0x18 0x25 0x80 b
    i2cset -y 1 0x18 0x26 0x80 b
    i2cset -y 1 0x18 0x27 0x80 b
    i2cset -y 1 0x18 0x00 0x00 b
    i2cset -y 1 0x18 0x3f 0xd8 b
    i2cset -y 1 0x18 0x40 0x00 b
    i2cset -y 1 0x18 0x41 0x00 b
    i2cset -y 1 0x18 0x42 0x00 b
    echo "PI_AI_MIC DAC route: headphone stereo"
}

speaker_mute() {
    i2cset -y 1 0x18 0x00 0x00 b >/dev/null 2>&1 || true
    i2cset -y 1 0x18 0x40 0x0c b >/dev/null 2>&1 || true
    configure_pcal_output "$OUT_HEADPHONE"
}

speaker_unmute() {
    i2cset -y 1 0x18 0x00 0x00 b >/dev/null 2>&1 || true
    i2cset -y 1 0x18 0x40 0x00 b >/dev/null 2>&1 || true
    configure_pcal_output "$OUT_SPEAKER"
}

write_xmos_led_pair() {
    local red_state="$1"
    local green_state="$2"

    [ "$LED_INDICATORS" = "1" ] || return 0

    python3 - "$red_state" "$green_state" <<'PY'
import fcntl
import os
import sys

I2C_SLAVE = 0x0703
XVF3800_I2C_ADDR = 0x2C
GPO_RESID = 0x14
GPO_PIN_VAL = 0x01

fd = os.open("/dev/i2c-1", os.O_RDWR)
try:
    fcntl.ioctl(fd, I2C_SLAVE, XVF3800_I2C_ADDR)
    for pin, state in ((6, int(sys.argv[1])), (7, int(sys.argv[2]))):
        payload = bytes([0, pin, 1 if state else 0])
        packet = bytes([GPO_RESID, GPO_PIN_VAL, len(payload)]) + payload
        os.write(fd, packet)
        status = os.read(fd, 1)[0]
        if status != 0:
            raise SystemExit(status)
finally:
    os.close(fd)
PY
}

speaker_sink_index() {
    local name="${ACTIVE_SINK_NAME:-$SINK_NAME}"
    pactl list sinks short 2>/dev/null |
        awk -F '\t' -v name="$name" '$2 == name { print $1; exit }'
}

speaker_playback_active() {
    local sink_index
    sink_index="$(speaker_sink_index)"
    [ -n "$sink_index" ] || return 1
    pactl list sink-inputs short 2>/dev/null |
        awk -F '\t' -v sink="$sink_index" '$2 == sink { found = 1 } END { exit found ? 0 : 1 }'
}

apply_state() {
    local input="$1"
    local playback_active="$2"
    local jack=0
    local mic_square=0

    if [ -n "$input" ] && [ $((input & P5_JACK_MASK)) -ne 0 ]; then
        jack=1
    fi
    if [ -n "$input" ] && [ $((input & P6_MIC_MASK)) -ne 0 ]; then
        mic_square=1
    fi

    write_xmos_led_pair "$jack" "$mic_square" >/dev/null 2>&1 || true

    if [ "$jack" = "1" ]; then
        configure_pcal_output "$OUT_HEADPHONE"
        configure_dac_headphone
        echo "PI_AI_MIC route: input=${input:-unknown} output=0x05 mode=headphone mic_square=$mic_square"
    elif [ "$playback_active" = "1" ]; then
        configure_dac_speaker_diff_muted
        speaker_unmute
        echo "PI_AI_MIC route: input=${input:-unknown} output=0x85 mode=speaker_active mic_square=$mic_square"
    else
        configure_dac_speaker_diff_muted
        speaker_mute
        echo "PI_AI_MIC route: input=${input:-unknown} output=0x05 mode=speaker_idle mic_square=$mic_square"
    fi
}

monitor_jack_and_playback() {
    local previous_sense=""
    local previous_playback=""
    local input
    local sense
    local playback

    while true; do
        input="$(read_pcal_input)"
        if [ -n "$input" ]; then
            sense=$((input & (P5_JACK_MASK | P6_MIC_MASK)))
        else
            sense=""
        fi
        if speaker_playback_active; then
            playback=1
        else
            playback=0
        fi

        # Ignore output-pin readback changes (notably AMP_CTRL2/P7). Only
        # P5/P6 are physical sense inputs and may trigger route reconfiguration.
        if [ "$sense" != "$previous_sense" ] || [ "$playback" != "$previous_playback" ]; then
            apply_state "$input" "$playback"
            previous_sense="$sense"
            previous_playback="$playback"
        fi

        sleep "$POLL_INTERVAL"
    done
}

main() {
    local card
    wait_for_pactl
    card="$(wait_for_xmos_card)"
    run_board_init
    configure_xvf_minimal
    # The XMOS reference init script uses a different PCAL mapping. Restore a
    # safe PI_AI_MIC state before PipeWire work so a later failure cannot leave
    # the speaker amp or jack route in an undefined state.
    configure_pcal_output "$OUT_HEADPHONE"
    load_pipewire_nodes "$card"
    apply_state "$(read_pcal_input)" 0
    if [ "${1:-monitor}" = "monitor" ]; then
        monitor_jack_and_playback
    fi
}

main "${1:-monitor}"

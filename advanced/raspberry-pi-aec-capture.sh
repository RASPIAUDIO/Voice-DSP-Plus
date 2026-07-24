#!/usr/bin/env bash
set -euo pipefail

PROFILE="${1:-}"
BEAT="${2:-}"
OUTPUT="${3:-}"
PLAYBACK_DEVICE="${VOICE_DSP_PLAYBACK_DEVICE:-pulse}"
CAPTURE_DEVICE="${VOICE_DSP_CAPTURE_DEVICE:-hw:2,0}"

if [[ -z "$PROFILE" || -z "$BEAT" || -z "$OUTPUT" ]]; then
  echo "Usage: voice-dsp-plus-aec-capture raw|linear|full BEAT.wav OUTPUT.wav" >&2
  exit 2
fi
[[ -f "$BEAT" ]] || { echo "Beat file not found: $BEAT" >&2; exit 2; }

case "$PROFILE" in
  raw|linear|full) ;;
  bypass)
    echo "WARNING: 'bypass' is deprecated; using the click-free raw profile." >&2
    PROFILE=raw
    ;;
  *) echo "Unknown AEC profile: $PROFILE" >&2; exit 2 ;;
esac

mkdir -p "$(dirname "$OUTPUT")"
if [[ "${VOICE_DSP_SKIP_PROFILE_APPLY:-0}" != "1" ]]; then
  voice-dsp-plus-aec-profile apply "$PROFILE"
  sleep 1
fi

DURATION="$(python3 - "$BEAT" <<'PY'
import sys, wave
with wave.open(sys.argv[1], 'rb') as source:
    print((source.getnframes() + source.getframerate() - 1) // source.getframerate())
PY
)"

cleanup() {
  [[ -n "${PLAY_PID:-}" ]] && kill "$PLAY_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "Recording $PROFILE from $CAPTURE_DEVICE for ${DURATION}s to $OUTPUT"
aplay -q -D "$PLAYBACK_DEVICE" "$BEAT" &
PLAY_PID=$!
arecord -q -D "$CAPTURE_DEVICE" -f S32_LE -r 16000 -c 2 -d "$DURATION" "$OUTPUT"
wait "$PLAY_PID"
PLAY_PID=""
echo "Capture complete: $OUTPUT"
voice-dsp-plus-aec-profile show

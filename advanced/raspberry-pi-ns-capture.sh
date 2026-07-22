#!/usr/bin/env bash
set -euo pipefail

PROFILE="${1:-}"
OUTPUT="${2:-}"
DURATION="${3:-12}"

if [[ -z "$PROFILE" || -z "$OUTPUT" ]]; then
  echo "Usage: voice-dsp-plus-ns-capture PROFILE OUTPUT.wav [SECONDS]" >&2
  exit 2
fi

case "$PROFILE" in
  no-processing|recommended|natural|baseline|balanced|strong|xmos-hybrid|xmos-hybrid-relaxed|xmos-hybrid-open) ;;
  *)
    echo "Unknown profile: $PROFILE" >&2
    exit 2
    ;;
esac

mkdir -p "$(dirname "$OUTPUT")"
voice-dsp-plus-ns-profile apply "$PROFILE"
sleep 1

echo "Recording $PROFILE for $DURATION seconds to $OUTPUT"
arecord -q -D pulse -f S32_LE -r 16000 -c 2 -d "$DURATION" "$OUTPUT"

echo "Capture complete: $OUTPUT"
voice-dsp-plus-ns-profile show

#!/usr/bin/env bash
set -euo pipefail

XVF_DIR="${XVF_DIR:-/opt/voice-dsp-plus/host-control-runtime}"
XVF_HOST="${XVF_HOST:-${XVF_DIR}/xvf_host}"

run_xvf() {
  local attempt output
  for attempt in $(seq 1 30); do
    if output=$(cd "$XVF_DIR" && "$XVF_HOST" --use i2c "$@" 2>&1); then
      printf '%s\n' "$output"
      sleep 0.6
      return 0
    fi
    sleep 1
  done

  printf 'xvf_host failed after %d attempts: %s\n%s\n' "$attempt" "$*" "$output" >&2
  return 1
}

# Far-field profile validated with the four-utterance jarvis.ogg test at 3 m.
run_xvf PP_MIN_NS 0.10
run_xvf PP_MIN_NN 0.51
run_xvf PP_AGCONOFF 1
run_xvf AUDIO_MGR_MIC_GAIN 10
run_xvf PP_LIMITONOFF 1
run_xvf PP_LIMITPLIMIT 0.47

# Seed the adaptive AGC at a clean value. PP_AGCGAIN then evolves at runtime.
run_xvf PP_AGCGAIN 10

echo "Applied Voice DSP+ Hey Jarvis far-field profile"

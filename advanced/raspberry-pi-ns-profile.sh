#!/usr/bin/env bash
set -euo pipefail

XVF_DIR="${XVF_DIR:-/opt/voice-dsp-plus/host-control-runtime}"
XVF_HOST="${XVF_HOST:-${XVF_DIR}/xvf_host}"

usage() {
  cat <<'EOF'
Usage:
  voice-dsp-plus-ns-profile show
  voice-dsp-plus-ns-profile apply no-processing|recommended|natural|baseline|balanced|strong
  voice-dsp-plus-ns-profile apply xmos-hybrid|xmos-hybrid-relaxed|xmos-hybrid-open
  voice-dsp-plus-ns-profile apply strong-gate-light|strong-gate-medium|strong-gate-hard

The no-processing profile keeps the same beam and routing but neutralizes noise
and echo suppression, AGC, ATTNS and the limiter. The other profiles restore
those blocks before applying their noise-suppression settings. Lower gain-floor
and nominal values apply stronger suppression and may sound less natural.
EOF
}

run_xvf() {
  (cd "$XVF_DIR" && "$XVF_HOST" --use i2c "$@")
  # The XVF host-control queue needs time to drain between short commands.
  sleep 0.6
}

show() {
  run_xvf PP_MIN_NS
  run_xvf PP_MIN_NN
  run_xvf PP_ATTNS_MODE
  run_xvf PP_ATTNS_NOMINAL
  run_xvf PP_ATTNS_SLOPE
  run_xvf PP_ECHOONOFF
  run_xvf PP_NLATTENONOFF
  run_xvf PP_AGCONOFF
  run_xvf PP_LIMITONOFF
}

apply_profile() {
  local profile="$1"
  local min_ns
  local min_nn
  local attns_mode=0
  local attns_nominal=1.0
  local attns_slope=1.0
  local echo_on=1
  local nl_atten_on=1
  local agc_on=1
  local limit_on=1

  case "$profile" in
    no-processing)
      min_ns=1.0
      min_nn=1.0
      echo_on=0
      nl_atten_on=0
      agc_on=0
      limit_on=0
      ;;
    natural)
      min_ns=0.20
      min_nn=0.55
      ;;
    baseline)
      min_ns=0.15
      min_nn=0.51
      ;;
    balanced)
      min_ns=0.10
      min_nn=0.40
      ;;
    strong)
      min_ns=0.05
      min_nn=0.28
      ;;
    xmos-hybrid)
      min_ns=0.05
      min_nn=0.51
      attns_mode=1
      attns_nominal=0.40
      ;;
    xmos-hybrid-relaxed)
      min_ns=0.10
      min_nn=0.51
      attns_mode=1
      attns_nominal=0.40
      ;;
    recommended|xmos-hybrid-open)
      min_ns=0.10
      min_nn=0.51
      ;;
    strong-gate-light)
      min_ns=0.05
      min_nn=0.28
      attns_mode=1
      attns_nominal=0.65
      ;;
    strong-gate-medium)
      min_ns=0.05
      min_nn=0.28
      attns_mode=1
      attns_nominal=0.40
      ;;
    strong-gate-hard)
      min_ns=0.05
      min_nn=0.28
      attns_mode=1
      attns_nominal=0.25
      ;;
    *)
      echo "Unknown profile: $profile" >&2
      usage >&2
      exit 2
      ;;
  esac

  run_xvf PP_MIN_NS "$min_ns"
  run_xvf PP_MIN_NN "$min_nn"
  run_xvf PP_ATTNS_MODE "$attns_mode"
  run_xvf PP_ATTNS_NOMINAL "$attns_nominal"
  run_xvf PP_ATTNS_SLOPE "$attns_slope"
  run_xvf PP_ECHOONOFF "$echo_on"
  run_xvf PP_NLATTENONOFF "$nl_atten_on"
  run_xvf PP_AGCONOFF "$agc_on"
  run_xvf PP_LIMITONOFF "$limit_on"
  echo "Applied noise-suppression profile: $profile"
  show
}

case "${1:-}" in
  show)
    show
    ;;
  apply)
    [[ $# -eq 2 ]] || { usage >&2; exit 2; }
    apply_profile "$2"
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac

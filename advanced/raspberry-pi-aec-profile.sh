#!/usr/bin/env bash
set -euo pipefail

XVF_DIR="${XVF_DIR:-/opt/voice-dsp-plus/host-control-runtime}"
XVF_HOST="${XVF_HOST:-${XVF_DIR}/xvf_host}"

usage() {
  cat <<'EOF'
Usage:
  voice-dsp-plus-aec-profile show
  voice-dsp-plus-aec-profile apply raw|linear|full

All profiles keep SHF_BYPASS=0. The firmware's SHF bypass is a timing-test path
that causes repeating dropouts on this build and is not a clean AEC reference.
  raw:    post-mic-gain channels, no AEC in the selected output path
  linear: linear AEC residuals, non-linear echo attenuation off
  full:   processed output with the selected double-talk AEC profile
EOF
}

run_xvf() {
  (cd "$XVF_DIR" && "$XVF_HOST" --use i2c "$@")
  sleep 0.6
}

show() {
  run_xvf SHF_BYPASS
  run_xvf AEC_FAR_EXTGAIN
  run_xvf AEC_AECSILENCELEVEL
  run_xvf AEC_AECCONVERGED
  run_xvf AEC_AECPATHCHANGE
  run_xvf PP_ECHOONOFF
  run_xvf PP_NLATTENONOFF
  run_xvf PP_NLAEC_MODE
  run_xvf PP_MIN_NS
  run_xvf PP_MIN_NN
  run_xvf PP_ATTNS_MODE
  run_xvf AUDIO_MGR_OP_L
  run_xvf AUDIO_MGR_OP_R
}

apply_profile() {
  local profile="$1"
  local echo_on nl_atten_on mux_category

  case "$profile" in
    raw)
      echo_on=0
      nl_atten_on=0
      mux_category=3
      ;;
    linear)
      echo_on=1
      nl_atten_on=0
      mux_category=7
      ;;
    full)
      echo_on=1
      nl_atten_on=1
      mux_category=8
      ;;
    *)
      echo "Unknown AEC profile: $profile" >&2
      usage >&2
      exit 2
      ;;
  esac

  # Keep the validated scheduler and double-talk settings constant.
  run_xvf SHF_BYPASS 0
  run_xvf AEC_FAR_EXTGAIN 0
  run_xvf PP_MIN_NS 0.10
  run_xvf PP_MIN_NN 0.51
  run_xvf PP_ATTNS_MODE 0
  run_xvf PP_AGCONOFF 0
  run_xvf PP_AGCGAIN 10
  run_xvf PP_LIMITONOFF 1
  run_xvf PP_LIMITPLIMIT 0.47
  run_xvf PP_NLAEC_MODE 0
  run_xvf PP_DTSENSITIVE 10
  run_xvf PP_GAMMA_E 1
  run_xvf PP_GAMMA_ETAIL 1
  run_xvf PP_GAMMA_ENL 1.1
  run_xvf AUDIO_MGR_MIC_GAIN 10
  run_xvf AUDIO_MGR_REF_GAIN 1.5
  run_xvf PP_ECHOONOFF "$echo_on"
  run_xvf PP_NLATTENONOFF "$nl_atten_on"
  run_xvf AUDIO_MGR_OP_L "$mux_category" 0
  run_xvf AUDIO_MGR_OP_R "$mux_category" 1
  echo "Applied AEC profile: $profile"
  show
}

case "${1:-}" in
  show)
    show
    ;;
  apply)
    [[ $# -eq 2 ]] || { usage >&2; exit 2; }
    if [[ "$2" == "bypass" ]]; then
      echo "WARNING: 'bypass' now maps to the click-free raw profile." >&2
      apply_profile raw
    else
      apply_profile "$2"
    fi
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac

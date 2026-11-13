#!/bin/sh
# Matis94 115f3a0c0/89433220e: selected TrueHD and fixed DV audio require
# these compiled components. Old SDK images must fail before linking the app.
set -eu
root=${1:?FFmpeg prefix required}
for feature in DCA_DECODER TRUEHD_DECODER AC3_DECODER EAC3_DECODER AAC_ENCODER AC3_ENCODER; do
  if ! grep -qx "CONFIG_$feature=yes" "$root/nuvio-audio-config" 2>/dev/null; then
    echo "FFmpeg SDK lacks confirmed $feature; rebuild tools/Dockerfile (selected TrueHD/fixed DV audio required)" >&2
    exit 2
  fi
done

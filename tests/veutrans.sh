#!/bin/bash
# Render the real carousel and Detail shaders; fail on a veil discontinuity.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
out="${NV_VEU_OUT:-/Volumes/ExternalSSD/tmp/rev/veutrans/after}"
mkdir -p "$out"
export NV_BINARY="${NV_BINARY:-$TMPDIR/nuvio-veutrans-after-shot}"
for layout in 2 0 1; do
  for material in off frost glass; do
    dir="$out/L$layout-$material"
    mkdir -p "$dir"
    export NV_VEU_TRANS="$layout" NV_KEEP=1
    export NUVIO_SHOT_VIDRO=0
    export NV_AJ=$'fundoLocal 0\ntrailerAutomatico 1'
    if [ "$material" = frost ]; then export NV_AJ=$'fundoLocal 2\ntrailerAutomatico 1'; fi
    if [ "$material" = glass ]; then export NUVIO_SHOT_VIDRO=1; fi
    bash tests/carrossel_shot.sh "$dir" >"$dir/capture.log" 2>&1
    export NV_REUSE=1
  done
done
python3 tests/veutrans.py "$out"

#!/bin/bash
# Real GL regressions for resting material and far-right comment-row culling.
# Warm-cache runtime stays below the reviewer's 50-second quick-check limit.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/detalheglass/rev4
mkdir -p "$out/tmp"
export TMPDIR="$out/tmp" OBJ=/Volumes/ExternalSSD/tmp/detalheglass/obj
export NUVIO_DETAIL_CHECK=comments NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1
bash tests/detalhe_glass_shot.sh "$out" comments

#!/bin/bash
# Focused episode plate (zoomed-thumbnail width, padded text, glass) and the
# title <-> season tabs <-> episodes frame budget both ways, Moderna + Frost (#205).
#   NUVIO_EPS_FRAMES=1 traces each frame (fill, glass copy/blur, fill by mode).
#   bash tests/epfoco.sh        rc=0 passes; < 50 s with a warm object cache
set -eu
cd "$(dirname "$0")/.."
OUT=/Volumes/ExternalSSD/tmp/epfoco
export TMPDIR="$OUT/tmp" OBJ="$OUT/obj"
mkdir -p "$TMPDIR" "$OBJ"
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_SHOT_LAYOUT=0 NUVIO_SHOT_FUNDO=2
export NUVIO_SHOT_BORDA=${NUVIO_SHOT_BORDA:-1}   # owner TV: focus ring off, thumbnail zooms
NUVIO_DETAIL_CHECK=eps bash tests/detalhe_glass_shot.sh "$OUT" eps 2>&1 | grep -E '^(PASS|FAIL|title)|error'
exit "${PIPESTATUS[0]}"

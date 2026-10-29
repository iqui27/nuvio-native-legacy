#!/bin/bash
# Title-page "..." more menu on the shared glass+frost material.
#   bash tests/detmaisglass.sh [check]      frost under the panel (rc=0 passes)
#   bash tests/detmaisglass.sh before|after [glass...]   PNGs, glass 0 and/or 1
# before = detmais.c at e047eecee; other sources stay current.
set -eu
cd "$(dirname "$0")/.."
OUT=${NUVIO_DETAIL_OUT:-/Volumes/ExternalSSD/tmp/detalheglass/fix2}
export TMPDIR="$OUT/tmp"
export OBJ="${OBJ:-/Volumes/ExternalSSD/tmp/detalheglass/obj}"
mkdir -p "$OUT" "$TMPDIR" "$OBJ"
mode=${1:-check}
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1
if [ "$mode" = check ]; then
  NUVIO_DETAIL_CHECK=mais bash tests/detalhe_glass_shot.sh "$OUT" mais | grep -E '^(movie|series|PASS|FAIL)'
  exit "${PIPESTATUS[0]}"
fi
export NUVIO_DETAIL_SHOT=1
if [ "$mode" = before ]; then
  git show e047eecee:src/detmais.c > "$OUT/detmais-before.c"
  export NUVIO_DETMAIS_SOURCE="$OUT/detmais-before.c"
fi
shift || true
for glass in ${@:-0 1}; do
  dir="$OUT/mais-$mode/glass-$glass"
  NUVIO_SHOT_VIDRO=$glass bash tests/detalhe_glass_shot.sh "$dir" filme-mais serie-mais >/dev/null
  for bmp in "$dir"/*.bmp; do
    sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null
    rm -f "$bmp"
  done
done

#!/bin/bash
# Compare the approved 2.0.8 components with Detail 2.1; no device access.
# Optional arguments: scale (85 or 100), material (0 solid, 1 glass),
# stage (legacy, current, compare, or all).
set -euo pipefail
cd "$(dirname "$0")/.."
out=${NUVIO_PROOF_OUT:-/Volumes/ExternalSSD/tmp/rev/j6-r4}
export OBJ=${OBJ:-/tmp/nv21-obj-$(basename "$PWD")}
mkdir -p "$out/reference" "$out/current" "$out/legacy" "$out/compare"
for file in detail.c detail.h notasui.c musicaui.c; do
  git show "integracao/2.0.5:src/$file" > "$out/reference/$file"
done
export NUVIO_SHOT_SOURCE=1 NUVIO_SHOT_AMIGOS=1 NUVIO_SHOT_TIME=1792971600
export NUVIO_PROOF_ASSETS="$out/assets"
for scale in ${1:-85 100}; do
  for glass in ${2:-0 1}; do
    export NUVIO_PROOF_SCALE=$scale NUVIO_PROOF_GLASS=$glass
    if [ "${3:-all}" = all ] || [ "${3:-all}" = legacy ]; then
    timeout 60 env NUVIO_LEGACY_PROOF=1 \
      NUVIO_DETAIL_SOURCE="$out/reference/detail.c" \
      NUVIO_DETAIL_HEADER="$out/reference/detail.h" \
      NUVIO_LEGACY_NOTAS="$out/reference/notasui.c" \
      NUVIO_LEGACY_MUSIC="$out/reference/musicaui.c" \
      bash tests/detalhe_glass_shot.sh "$out/legacy" legacy-proof \
      > "$out/legacy-$scale-$glass.log" 2>&1
    fi
    if [ "${3:-all}" = all ] || [ "${3:-all}" = current ]; then
    timeout 60 env NUVIO_DETAIL21_PROOF=1 \
      bash tests/detalhe_glass_shot.sh "$out/current" legacy-proof \
      > "$out/current-$scale-$glass.log" 2>&1
    fi
    if [ "${3:-all}" = all ] || [ "${3:-all}" = compare ]; then
    material=solid; [ "$glass" = 0 ] || material=glass
    prefix="$scale-$material"
    for before in "$out/legacy/$prefix"-*.png; do
      name=$(basename "$before")
      magick "$before" "$out/current/$name" +append "$out/compare/$name"
    done
    fi
    echo "PASS: scale=$scale glass=$glass stage=${3:-all}"
  done
done

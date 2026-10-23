#!/bin/bash
# Focused regression checks; optional baseline source, no checkout changes.
set -eu
cd "$(dirname "$0")/.."
out=${NUVIO_DETAIL_OUT:-/Volumes/ExternalSSD/tmp/detalheglass/fix2}
mkdir -p "$out/tmp" "$out/obj"
export TMPDIR="$out/tmp" OBJ=/Volumes/ExternalSSD/tmp/detalheglass/obj
export NUVIO_DETAIL_CHECK=cards NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_SHOT_LAYOUT=2
status=0
for glass in 1 0; do
  NUVIO_SHOT_VIDRO=$glass bash tests/detalhe_glass_shot.sh "$out" cards > "$out/cards-$glass.log" 2>&1 || status=1
  grep -E '^(PASS|FAIL)' "$out/cards-$glass.log"
done
exit "$status"

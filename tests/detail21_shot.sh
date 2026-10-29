#!/bin/bash
# Native 1920x1080 deterministic fixtures; never accesses a TV or account.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/detalhe21-resto-png
export OBJ=/tmp/nv21-resto-shot-obj
export NUVIO_SHOT_SOURCE=1
for mode in 0 1 2; do
  case "$mode" in 0) name=ready;; 1) name=personal;; 2) name=both;; esac
  NUVIO_SHOT_TOPO="$mode" bash tests/detalhe_glass_shot.sh "$out/after-$name" \
    filme-topo serie-topo filme-notas filme-elenco filme-comentarios filme-fim serie-ep serie-progresso serie-fim
 done
NUVIO_SHOT_TOPO=0 NUVIO_SHOT_SPOILERS=1 bash tests/detalhe_glass_shot.sh "$out/after-spoilers" serie-ep filme-comentarios
for dir in "$out"/before "$out"/after-*; do
  [ -d "$dir" ] || continue
  for bmp in "$dir"/*.bmp; do
    [ -f "$bmp" ] || continue
    sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null
  done
done

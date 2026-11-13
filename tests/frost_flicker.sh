#!/bin/bash
# Hidden desktop GL; real TV layout/renderer, isolated offline data, SDL input.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${NUVIO_FROST_TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
export OBJ="${NUVIO_FROST_OBJ:-$TMPDIR/nuvio-frostpisca-obj}"
export NUVIO_C9_CACHE=1 NUVIO_FROST_FLICKER=1
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_SHOT_VIDRO_QUAL=2
out="${NUVIO_FROST_OUT:-/Volumes/ExternalSSD/tmp/rev/frostpisca}/${1:-verified}"
mkdir -p "$out" "$OBJ"
# Reuse the existing object's cache. No application launch or desktop activation.
NUVIO_FF_COMPILE_ONLY=1 bash tests/detalhe_glass_shot.sh "$out" > "$out/build.log" 2>&1
rc=0
for layout in 0 1 2; do
  data=$(mktemp -d "$TMPDIR/nuvio-detglass-dados.XXXXXX")
  if NUVIO_DADOS="$data" NUVIO_SHOT_LAYOUT=$layout SDL_MAC_BACKGROUND_APP=1 \
     "$OBJ/shot" "$out" > "$out/layout$layout.log" 2>&1; then result=0; else result=$?; fi
  rm -rf "$data"
  printf '%s\n' "$result" > "$out/layout$layout-rc.txt"
  if [ "$result" -ne 0 ]; then rc=1; fi
  rg '^(PASS|FAIL|CAPTURE)' "$out/layout$layout.log" || true
  for scene in region static hero row posters detail; do
    magick "$out/layout$layout-$scene-0"{0,1,2,3,4,5,6,7}.png \
      -resize 480x270 +append "$out/layout$layout-$scene-strip.png"
  done
done
exit "$rc"

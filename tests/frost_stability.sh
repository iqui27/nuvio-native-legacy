#!/bin/bash
# Native frame-to-frame regression. No accounts, network workers or device writes.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
export OBJ="$TMPDIR/nuvio-frostpisca-obj"
export NUVIO_C9_CACHE=1 NUVIO_FROST_STABILITY=1
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_SHOT_VIDRO_QUAL=2 NUVIO_SHOT_FUNDO=2
out="/Volumes/ExternalSSD/tmp/rev/frostpisca/${1:-after}"
mkdir -p "$out" "$OBJ"
cc -O1 -g -DNV_WEBOS -DNV_SHOT_HOOKS -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined -c src/gfx.c -o "$OBJ/gfx.o"
touch "$OBJ/gfx.o"
set +e
bash tests/detalhe_glass_shot.sh "$out" stability
rc=$?
printf '%s\n' "$rc" > "$out/rc.txt"
for screen in home detail library; do
  for phase in static focus; do
    magick "$out/$screen-$phase-0.png" "$out/$screen-$phase-1.png" \
      "$out/$screen-$phase-2.png" "$out/$screen-$phase-3.png" \
      -resize 480x270 +append "$out/$screen-$phase-strip.png"
  done
done
exit "$rc"

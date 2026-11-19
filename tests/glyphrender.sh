#!/bin/bash
# One offline regression; baseline must fail, current renderer must pass.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/glyphrender
mkdir -p "$out"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
baseline=fab11c7bf
# Keep original source/header/atlas together; never modify the checkout for red proof.
# Baseline predates the clip arguments (partial culling); accept and ignore them.
git show "$baseline:src/text.c" |
  sed -e 's/int gray, float alpha, int \*lines) {/int gray, float alpha, int *lines, float clipTop, float clipBottom) { (void)clipTop; (void)clipBottom;/' \
      -e 's/^\( *int gray, float alpha\)) {$/\1, float clipTop, float clipBottom) { (void)clipTop; (void)clipBottom;/' > "$work/text.c"
git show "$baseline:src/emoji_atlas.h" > "$work/emoji_atlas.h"
mkdir -p "$work/resources/fonts"
for font in "$PWD"/deploy/app/fonts/*; do
  [ "$(basename "$font")" = addon-emoji.png ] || ln -s "$font" "$work/resources/fonts/"
done
git show "$baseline:deploy/app/fonts/addon-emoji.png" > "$work/resources/fonts/addon-emoji.png"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/streams.c|src/text.c) ;; *) sources+=("$source");; esac
done
export NV_ADDON_FIXTURE=tests/fixtures/glyphrender.json
export NUVIO_SEM_RESERVA_DE_SISTEMA=1
export NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data"
mkdir -p "$NUVIO_DADOS"
flags=(-DNV_SHOT_HOOKS -DNV_ADDON_LAYOUT_TEST -DNV_GLYPH_RENDER_TEST
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""'
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""'
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""'
  -I"$PWD/src" -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
# Compile common code once; only text.c differs between red and green proof.
absolute=()
for source in "${sources[@]}" tests/fonte_addon_texto_shot.c tests/glyphrender.c; do
  absolute+=("$PWD/$source")
done
(cd "$work"; cc "${flags[@]}" -c "${absolute[@]}") > "$out/common-compile.log" 2>&1
for mode in before after; do
  text=src/text.c
  export NV_ADDON_RESOURCES=deploy/app
  if [ "$mode" = before ]; then text="$work/text.c"; export NV_ADDON_RESOURCES="$work/resources"; fi
  cc "${flags[@]}" "$work"/*.o "$text" -o "$work/shot" \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    > "$out/$mode-compile.log" 2>&1
  rc=0
  "$work/shot" "$out/$mode" > "$out/$mode.log" 2>&1 || rc=$?
  if [ "$mode" = before ]; then
    [ "$rc" -eq 1 ] || { echo "FAIL: baseline expected rc 1, got $rc"; exit 1; }
    echo 'PASS: baseline fails on incomplete ZWJ cluster (rc 1)'
  else
    [ "$rc" -eq 0 ] || { echo "FAIL: current renderer rc $rc; see $out/after.log"; exit 1; }
    echo 'PASS: current parser, glyph coverage, rasterization and wrapping'
  fi
done
magick montage -label Before "$out/before-1-foco-primeira.png" \
  -label After "$out/after-1-foco-primeira.png" \
  -background '#111318' -fill white -font deploy/app/fonts/InterDisplay-Regular.ttf -pointsize 18 \
  -geometry 960x540+0+8 -tile 1x2 "$out/source-sheet-before-after.png"
echo "Capture: $out/source-sheet-before-after.png"

#!/bin/bash
# Reproducible Home/detail proof with local art, no IMDb metadata requests.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-artepastel-shots.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p docs/shots/artepastel
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/corviva.c) continue;; esac
  sources+=("$root/$source")
done
(
  cd "$work"
  cc -c "${sources[@]}" "$root/tests/corviva_shot.c" -I"$root/src" -O0 \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations
)
git show f246d8e91:src/corviva.c > "$work/corviva-before.c"
for version in before after; do
  source="$work/corviva-before.c"
  [ "$version" = after ] && source=src/corviva.c
  cc "$source" "$work"/*.o -Isrc -o "$work/$version" -O0 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
 done
for mode in before vivid light; do
  binary=after
  [ "$mode" = before ] && binary=before
  mkdir "$work/nuvio-corviva-shot-$mode"
  NUVIO_DADOS="$work/nuvio-corviva-shot-$mode" \
    NUVIO_ARTEPASTEL_SHOT=1 NUVIO_ARTEPASTEL_LIGHT=$([ "$mode" = light ] && echo 1 || true) \
    "$work/$binary" "$work/$mode" > "$work/$mode.log" 2>&1
  for screen in home detail; do
    bmp="$work/$mode-detail.bmp"
    [ "$screen" = home ] && bmp="$work/$mode-1-home-dinamica-vermelho.bmp"
    sips -s format png "$bmp" --out "docs/shots/artepastel/$mode-$screen.png" >/dev/null
  done
  tail -1 "$work/$mode.log"
done
for screen in home detail; do
  cmp "$work/before-$([ "$screen" = home ] && echo 1-home-dinamica-vermelho || echo detail).bmp" \
      "$work/vivid-$([ "$screen" = home ] && echo 1-home-dinamica-vermelho || echo detail).bmp"
  echo "PASS: $screen Vivid pixels identical to baseline"
done

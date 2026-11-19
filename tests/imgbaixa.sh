#!/bin/bash
# Offline C9: compare native 1080p art with the pre-upscaler GPU path.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/imgbaixa}"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-imgbaixa.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$out/before" "$out/after" "$work/data" "$work/src/dts" "$work/tests"
printf 'animacoes 0\nidioma 1\nhomeLayoutLocal 0\nampliarAlgoLocal 4\nampliarNitidezLocal 4\n' > "$work/data/ajustes.txt"
git show e53b13c06:src/gpunivel.c > "$work/baseline.c"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/gfx.c|src/gpunivel.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
printf '%s\0' "${sources[@]}" tests/ui720_settings.c |
  xargs -0 -P 8 -I '{}' cc '{}' -c -o "$work/{}.o" -DAJUSTES_TESTE -Isrc -O0 \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined
for version in before after; do
  flags=()
  if [ "$version" = before ]; then flags+=("-DIMGBAIXA_BASELINE=\"$work/baseline.c\""); fi
  cc tests/ui720.c -c -o "$work/tests/ui720.c.o" -DAJUSTES_TESTE "${flags[@]}" -Isrc -O0 \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined
  cc "$work"/src/*.o "$work"/src/dts/*.o "$work"/tests/*.o -o "$work/test" \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
  NUVIO_SHOT_C9=1 NUVIO_DADOS="$work/data" "$work/test" "$out/$version" > "$out/$version/results.txt" 2>&1
  rg '^(PASS C9|PNG )' "$out/$version/results.txt"
done
for art in poster backdrop; do
  # Exact equality is stricter than a sharpness threshold.
  magick compare -metric AE "$out/before/c9-$art.png" "$out/after/c9-$art.png" null: 2> "$out/$art-ae.txt"
  echo "PASS C9 $art equals e53b13c06 (AE=0)"
done

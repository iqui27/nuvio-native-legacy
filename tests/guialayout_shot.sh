#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/guialayout/v2
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guialayout-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
cache="${TMPDIR:-/tmp}/nuvio-guialayout-$(basename "$PWD")"
mkdir -p "$cache" "$out/before" "$out/after"
export NUVIO_DADOS="$work/dados" GUIALAYOUT_ROOT="$PWD"
mkdir -p "$NUVIO_DADOS"
flags=(-Isrc -O0 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  object="$cache/$(basename "$source").o"
  if [[ ! -f "$object" || "$source" -nt "$object" ]]; then cc "${flags[@]}" -c "$source" -o "$object"; fi
  objects+=("$object")
done
git show integracao/2.1:src/guiav5ui.c > "$work/before.c"
cc "${flags[@]}" -DGUIALAYOUT_BASELINE -DGUIAV5_UNDER_TEST=\""$work/before.c"\" \
  tests/guialayout_shot.c "${objects[@]}" -o "$work/before" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/before" "$out/before"
rm -rf "$NUVIO_DADOS"
mkdir -p "$NUVIO_DADOS"
cc "${flags[@]}" tests/guialayout_shot.c "${objects[@]}" -o "$work/after" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/after" "$out/after"
python3 tests/guialayout_compare.py

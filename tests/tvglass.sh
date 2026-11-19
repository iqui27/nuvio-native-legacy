#!/bin/bash
# Offline GL regression and PNG proof; isolated settings, never device deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/tvglass/after}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tvglass.XXXXXX")
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
trap 'rm -rf "$work"' EXIT
flags=(-O1 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/menu.c|src/biblioteca.c|src/guia.c) continue;; esac
  objects+=("$source")
done
cc "${flags[@]}" -DNV_TVGLASS_LIBRARY -c tests/tvglass.c -o "$work/library.o"
cc "${flags[@]}" -DNV_TVGLASS_GUIDE -c tests/tvglass.c -o "$work/guide.o"
cc "${flags[@]}" "${objects[@]}" tests/tvglass.c "$work/library.o" "$work/guide.o" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/test"
"$work/test" "$out"

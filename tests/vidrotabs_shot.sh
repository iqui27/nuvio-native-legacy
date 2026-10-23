#!/bin/bash
# Pixel proof for the actual Library and catalog tab renderers, offline.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/vidrotabs/after}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vidrotabs.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/biblioteca.c|src/vertudo.c) ;; *) sources+=("$source");; esac
done
flags=(-O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Isrc
       -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
# Reuse common objects across both screen checks.
for source in "${sources[@]}"; do
  cc "${flags[@]}" -c "$source" -o "$work/$(basename "${source%.c}").o"
done
rc=0
for screen in library catalog; do
  mkdir -p "$work/$screen"
  if [ "$screen" = library ]; then
    cc "${flags[@]}" -DSHOT_LIBRARY tests/vidrotabs_shot.c src/vertudo.c "$work/"*.o "${libs[@]}" -o "$work/shot"
  else
    cc "${flags[@]}" tests/vidrotabs_shot.c src/biblioteca.c "$work/"*.o "${libs[@]}" -o "$work/shot"
  fi
  NUVIO_DADOS="$work/$screen" "$work/shot" "$out" || rc=1
done
exit "$rc"

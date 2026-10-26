#!/bin/bash
# Link the real 2.1 Library handlers with the existing native test objects.
set -euo pipefail
cd "$(dirname "$0")/.."
OBJ=/tmp/nv21-obj-$(basename "$PWD")
if [ ! -f "$OBJ/app.o" ]; then bash tests/detail21.sh; fi
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-biblioteca-up.XXXXXX")
trap 'rm -rf "$work"' EXIT
objects=()
for object in "$OBJ"/*.o; do
  case "$object" in */biblioteca.o) continue;; esac
  objects+=("$object")
done
cc -O1 -g tests/biblioteca_up.c src/detail.c "${objects[@]}" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/test"
NUVIO_DADOS="$work/dados" "$work/test"

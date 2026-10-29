#!/bin/bash
# Build native screenshot harness once; never starts account/network services.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arabe-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
out="${1:-/Volumes/ExternalSSD/tmp/rev/arabe-ui}"
mkdir -p "$out" "$work/data"
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/arabe_ui_shot.c -Isrc -o "$work/shot" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work/data" NUVIO_SEM_RESERVA_DE_SISTEMA=1 "$work/shot" "$out"

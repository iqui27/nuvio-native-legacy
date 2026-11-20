#!/bin/bash
# #657: real GL Light captures in separate processes sharing disposable storage.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-light-launch.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=${NUVIO_LUZ_OUT:-/Volumes/ExternalSSD/tmp/rev/ruidoperfil-png/after}
mkdir -p "$(dirname "$out")" "$work/data"
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/perfil_luz_arranque.c -Isrc -o "$work/test" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
rc=0
for run in first second; do
  NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" \
    "$work/test" "$out-$run-clean.png" "$out-$run.png" "$work/first.bmp" || rc=$?
done
exit "$rc"

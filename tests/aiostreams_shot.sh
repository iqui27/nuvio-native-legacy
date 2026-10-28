#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/aiostreams-png}
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
if [ "${AIO_SHOT_BIN:-}" ]; then
  mkdir -p "$out"
  printf 'idioma 0\nvidroLocal 0\nfonteInterface 0\n' > "$work/ajustes.txt"
  NUVIO_DADOS="$work" NUVIO_TESTE_DIR="$work" "$AIO_SHOT_BIN" "$out"
  exit
fi
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/aiostreams_shot.c -Isrc -o "$work/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$out"
printf 'idioma 0\nvidroLocal 0\nfonteInterface 0\n' > "$work/ajustes.txt"
NUVIO_DADOS="$work" NUVIO_TESTE_DIR="$work" "$work/shot" "$out"
cp "$work/shot" /tmp/nuvio-aiostreams-shot

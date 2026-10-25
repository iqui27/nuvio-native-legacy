#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-sources-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/fontes_cabecalho_shot.c -Isrc -o "$work/shot" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
output=/Volumes/ExternalSSD/tmp/rev/servidores-sources-png
mkdir -p "$output" "$work/data"
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" NUVIO_SHOT_IDIOMA=0 NUVIO_SHOT_SERVIDORES=1 "$work/shot" "$output/native"
mv "$output/native-1-lista.png" "$output/fontes-servidor-app.png"
python3 tools/servidores_png_check.py "$output"

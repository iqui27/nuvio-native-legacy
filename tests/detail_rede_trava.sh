#!/bin/bash
# Headless Detail frame progress with a silent local add-on.
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-detail-rede.XXXXXX")
trap 'rm -rf "$D"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/extras.c) continue;; esac
  sources+=("$source")
done
cc -c src/extras.c -Isrc -o "$D/extras.o" -O1 -g \
  -Ddesc_chave_tmdb=detailExtrasKey -Drede_baixar=detailExtrasDownload \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
cc "${sources[@]}" "$D/extras.o" tests/detail_rede_trava.c -Isrc -o "$D/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$D" "$D/test"

#!/bin/bash
# 2.1 tela 4: texto da faixa "Vai tocar", cartao da escolha e copias do mesmo
# arquivo. Ver tests/vaitocar.c.
#   bash tests/vaitocar.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
bin="${TMPDIR:-/tmp}/nuvio-vaitocar"
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/vaitocar.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
trap 'rm -f "$bin"' EXIT
"$bin"

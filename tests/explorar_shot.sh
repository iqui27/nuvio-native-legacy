#!/bin/bash
# Captura visual da tela Explorar. O catalogo e sintetico e usa arte local;
# nao toca conta, cache de usuario nem rede. A captura exige janela GL e olho
# humano, portanto fica fora da suite automatica.
set -eu
cd "$(dirname "$0")/.."
out="${1:-/tmp/nuvio-explorar}"
sources=()
for source in src/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/explorar_shot.c -Isrc -o /tmp/nuvio-explorar-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-explorar-shot "$out"

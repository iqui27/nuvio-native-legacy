#!/bin/bash
# Folha de Fontes 2.1: cartao da escolha automatica, copias dobradas e foco do
# D-pad. Janela GL: nao entra na suite de unitarios. Ver tests/titulo21_shot.c.
#   bash tests/titulo21_shot.sh tests/out/titulo21
# Gera <prefixo>-{1-todos,2-foco-fundo,3-foco-topo,4-aba-addon}.png.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
bin="${TMPDIR:-/tmp}/nuvio-titulo21-shot"
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/titulo21_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
mkdir -p "$(dirname "$1")"
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$bin" "$1"

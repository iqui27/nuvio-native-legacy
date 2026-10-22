#!/bin/bash
# #355: modal "Detalhes da fonte" (abre com I/F5/AMARELA, limpa o texto, nunca
# mostra a url, Voltar/OK fecham so ela). Ver tests/fonte_detalhe.c.
#
#   bash tests/fonte_detalhe.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
bin="${TMPDIR:-/tmp}/nuvio-fonte-detalhe"
cc "${sources[@]}" tests/fonte_detalhe.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$bin"

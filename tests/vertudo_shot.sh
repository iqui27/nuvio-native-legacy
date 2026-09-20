#!/bin/bash
# Abas da pagina de colecao nos dois estados, em BMP. Nao entra na suite
# (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL e de olho humano.
#
#   bash tests/vertudo_shot.sh /tmp/nuvio-vertudo
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-vertudo-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
# src/vertudo.c e INCLUIDO pelo teste: desenharCard e a lista de canais sao
# estaticos, e semear por dentro e o unico jeito de fotografar sem addon no ar.
cc "${sources[@]}" tests/vertudo_shot.c -Isrc -o /tmp/nuvio-vertudo-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-vertudo-shot "$@"

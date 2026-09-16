#!/bin/bash
# Capturas dos paineis de frases e de ficha, em BMP, sem rede e sem interacao.
# Nao entra na suite (o testa-tudo.sh pula *_shot.sh): precisa de janela GL e de
# olho humano para julgar.
#
#   bash tests/seriefrases_shot.sh /tmp/nuvio-seriefrases
#
# Compila tudo MENOS src/seriefrases.c, que a captura inclui para poder carregar os
# numeros medidos direto nos vetores do modulo — sem rede, sem fio e sem cache.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/seriefrases.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/seriefrases_shot.c -Isrc -o /tmp/nuvio-seriefrases-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-seriefrases-shot "$@"

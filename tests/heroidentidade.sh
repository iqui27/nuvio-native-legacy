#!/bin/bash
# O hero desenha a arte do titulo em foco, nao a do anterior (issue #118).
# Ver tests/heroidentidade_home.c.
set -eu
cd "$(dirname "$0")/.."
cc src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c tests/heroidentidade_home.c \
  -Isrc -o /tmp/nuvio-heroidentidade -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-heroidentidade

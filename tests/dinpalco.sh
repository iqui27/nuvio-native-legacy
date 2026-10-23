#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/corviva.c tests/home_tex_duble.c src/cwordem.c src/cwretido.c src/curadoria.c tests/dinpalco.c tests/amigosfil_stub.c \
  -Isrc -o "${TMPDIR:-/tmp}/nuvio-dinpalco" -O1 -g -ffunction-sections -fdata-sections \
  -DCURADORIA_TESTE -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-dinpalco"
bash tests/home-trailer-timer.sh

#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Reusa o fixture de layout, sem inicializar SDL/AppKit nem video.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/homefil.c src/artehero.c tests/home_tex_duble.c src/cwordem.c src/cwretido.c tests/homefil_home.c tests/amigosfil_stub.c \
  -Isrc -o /tmp/nuvio-homefil-home-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
for mode in quota watched local repin; do /tmp/nuvio-homefil-home-tests "$mode"; done

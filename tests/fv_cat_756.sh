#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-issue756.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/data" "$work/home/.nuvio"
cc src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/homefil.c src/artehero.c src/corviva.c src/buscanorm.c tests/home_tex_duble.c src/cwordem.c src/cwretido.c src/curadoria.c src/dados.c tests/fv_cat_756.c tests/amigosfil_stub.c tests/home_boundary_stub.c \
  -Isrc -o "$work/test" -O1 -g -ffunction-sections -fdata-sections \
  -DCURADORIA_TESTE -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
for scenario in write read other; do HOME="$work/home" NUVIO_DADOS="$work/data" "$work/test" "$scenario"; done

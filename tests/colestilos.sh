#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-colestilos.XXXXXXXX")"
trap 'rm -f "$bin"' EXIT
cc "${flags[@]}" src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/ajlog.c src/fonteregra.c src/regexjs.c src/posterprov.c src/redeurl.c src/colecoes.c src/colfileiras.c src/js.c src/catordem.c src/fileiras.c src/curadoria.c src/artehero.c src/cwordem.c src/cwretido.c tests/colestilos.c tests/amigosfil_stub.c -Wl,-dead_strip -o "$bin"
"$bin"

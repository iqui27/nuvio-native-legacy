#!/bin/bash
# Offline production geometry/picker regression; no SDL or network initialization.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-i616.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -ffunction-sections -fdata-sections -Wno-deprecated-declarations -Wno-macro-redefined)
cc "${flags[@]}" src/catalogo.c src/progresso.c src/focus.c src/ajustes.c \
  src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c \
  src/fileiras.c src/artehero.c src/corviva.c src/cwordem.c src/cwretido.c \
  src/curadoria.c tests/i616.c tests/amigosfil_stub.c \
  -DCURADORIA_TESTE -Wl,-dead_strip -o "$work/home"
cc "${flags[@]}" -DI616_PICKER tests/i616.c -Wl,-dead_strip -o "$work/picker"
rc=0
"$work/home" || rc=1
"$work/picker" || rc=1
exit "$rc"

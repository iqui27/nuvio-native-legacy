#!/bin/bash
# #369 item 1: "Ocultar nao lancados" nas fileiras da Home e na grade de colecao.
#   bash tests/colfiltro.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}"
flags=(-O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/colfiltro.c src/js.c src/metaprov.c -o "$out/nuvio-colfiltro-tests"
"$out/nuvio-colfiltro-tests"

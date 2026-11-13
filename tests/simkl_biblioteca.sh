#!/bin/bash
# A biblioteca do Simkl por dentro (src/simkl.c): paginas juntadas, listas por
# estado, a janela de conferencia com o Simkl. Inclui o simkl.c, como o i610.
# Sem SDL, sem rede. SANITIZE=1 liga ASan/UBSan.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}"; out="${out%/}/nuvio-simkl-biblioteca"
cc ${flags[@]+"${flags[@]}"} tests/simkl_biblioteca.c src/simklbib.c src/simklanime.c src/js.c src/traktscrobble.c src/metaep.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -lpthread -o "$out"
"$out"

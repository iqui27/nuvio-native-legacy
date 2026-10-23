#!/bin/bash
# Regressao FASE 2: todos os cenarios devem PASS. Rede falsa, sem TV.
# CW205_SEM_HISTORICO=1 bash tests/cw205_diagnostico.sh trakt  -> controle PASS
# bash tests/cw205_diagnostico.sh montagem                  -> OFF PASS
# CW205_PROXIMO=1 bash tests/cw205_diagnostico.sh montagem   -> ON PASS
# CW205_MESMA_OBRA=1 bash tests/cw205_diagnostico.sh montagem -> OFF PASS (dedup)
# CW205_PAUSADO_MAIS_NOVO=1 bash tests/cw205_diagnostico.sh   -> controle PASS
# CW205_REF=581e8fb9 bash tests/cw205_diagnostico.sh         -> master FAIL
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cw205.XXXXXX")
trap 'rm -rf "$dir"' EXIT
case "${1:-trakt}" in
  trakt)
    srcdir=src
    if [ -n "${CW205_REF:-}" ]; then
      git archive "$CW205_REF" src | tar -x -C "$dir"
      srcdir="$dir/src"
    fi
    cc "$srcdir/trakt.c" tests/stub_fichameta.c "$srcdir/js.c" "$srcdir/metaprov.c" tests/cw205_diagnostico.c \
      -I"$srcdir" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
      -o "$dir/teste" -O1 -g -Wall -Wextra -Wl,-dead_strip -lpthread ;;
  montagem)
    cc -DCW205_MONTAGEM src/catalogo.c src/progresso.c src/cwordem.c src/cwretido.c \
      tests/cw205_diagnostico.c src/cotacat.c src/js.c src/metaprov.c src/colecoes.c \
      src/redeurl.c src/catordem.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
      -o "$dir/teste" -O1 -g -Wall -Wno-deprecated-declarations \
      -Wno-macro-redefined -Wno-unused-function ;;
  *) echo 'uso: bash tests/cw205_diagnostico.sh [trakt|montagem]' >&2; exit 2 ;;
esac
"$dir/teste"

#!/bin/bash
# Reproducoes da revisao: corte apos conclusao e fallback ON -> OFF.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cwleg.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g
       -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
case "${1:-todos}" in
  corte|fallback|todos) ;;
  *) echo 'uso: bash tests/cwleg.sh [corte|fallback|todos]' >&2; exit 2 ;;
esac
if [ "${1:-todos}" != fallback ]; then
  cc src/trakt.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/cwleg_corte.c \
    "${flags[@]}" -Wl,-dead_strip -lpthread -o "$work/corte"
  "$work/corte"
  REPRO_ON=1 "$work/corte"
fi
if [ "${1:-todos}" != corte ]; then
  cc src/catalogo.c src/progresso.c src/cwordem.c src/cwretido.c tests/cwleg_fallback.c \
    src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
    "${flags[@]}" -o "$work/fallback"
  "$work/fallback"
fi

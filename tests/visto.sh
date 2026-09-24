#!/bin/bash
# "Marcar como assistido" em Trakt, Simkl e conta Nuvio (visto.c) e a
# temporada inteira num pedido so (issue #108). Rede falsa em tests/visto.c;
# os modulos de verdade, sem SDL linkado, sem rede.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-visto-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/visto.c src/simkl.c src/syncprog.c src/progresso.c src/trakt.c src/cwordem.c \
  src/vistoep.c src/js.c src/jsw.c tests/visto.c -o "$tmp/visto" -lpthread
"$tmp/visto"

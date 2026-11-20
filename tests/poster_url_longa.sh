#!/bin/bash
# URL de cartaz comprida (#361): 600/1173/2000 bytes com query string do JSON do
# addon ao CatItem, ao pedido de textura (rede, chave, nome no disco), aos
# Salvos gravados e relidos, a biblioteca da conta e ao jornal da conta. Ver
# tests/poster_url_longa.c.
#
#   bash tests/poster_url_longa.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-poster-longa-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/descoberta.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/poster_url_longa.c -Isrc -o "$tmp/teste" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
cc -DPUL_PEND src/contapend.c src/vistoep.c src/js.c src/jsw.c tests/poster_url_longa.c \
  -Isrc -o "$tmp/pend" -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
rc=0
"$tmp/teste" || rc=1
"$tmp/pend" || rc=1
exit "$rc"

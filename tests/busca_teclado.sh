#!/bin/bash
# Search keyboard: pill->field gap at 80/100/130% and the ?!# page. ~40 s: the
# app objects build in parallel at -O0.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-busca-teclado.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
FLAGS=(-O0 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -DNV_TRAKT_CLIENT_ID='"chave-de-teste"'
  -Wno-deprecated-declarations -Wno-macro-redefined)
ls src/*.c src/dts/*.c | grep -v -E '^src/(main|busca)\.c$' | xargs -P 8 -I{} sh -c \
  'o="$1/$(echo "$2" | tr / _).o"; cc "${@:3}" -c "$2" -o "$o"' _ "$work" {} "${FLAGS[@]}"
cc "${FLAGS[@]}" ${BUSCA_C:+-DBUSCA_C="\"$BUSCA_C\""} ${SO_GAP:+-DSO_GAP} tests/busca_teclado.c "$work"/*.o -o "$work/t" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/t"

#!/bin/bash
# #205: arabe em Naskh Bold em todo peso (UI) e \b1 na legenda simples (ver .c).
# Compila em paralelo (-O0) para caber em < 50 s. Requer GL (Mac).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arabe-negrito.XXXXXX")
trap 'rm -rf "$work"' EXIT
F="-O0 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Isrc -w"
for source in src/*.c src/dts/*.c tests/arabe_negrito_padrao.c; do
  [ "$source" != src/main.c ] && echo "$source"
done | xargs -P 8 -I{} sh -c 'cc -c $0 '"$F"' -o '"$work"'/$(echo $0 | tr / _).o' {}
cc "$work"/*.o -o "$work/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
NUVIO_SEM_RESERVA_DE_SISTEMA=1 NUVIO_DADOS="$work" "$work/test" | grep -v "^\[t\]\|^fonte\|^reserva\|^\[leg\]"

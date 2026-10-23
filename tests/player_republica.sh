#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# #190: republica o catalogo com o player aberto. Liga o app inteiro menos o
# main, como tests/player.sh; nenhuma janela e aberta (nada aqui desenha).
cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/player_republica.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-player-republica-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-player-republica-tests "$@"


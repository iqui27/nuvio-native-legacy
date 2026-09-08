#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Liga o app inteiro menos o main, como tests/player.sh: ctxmenu.c fala com
# catalogo, trakt e gfx. Nenhuma janela e aberta: o teste so manda eventos.
#
cc "${flags[@]}" "${sources[@]}" tests/hold.c -Isrc -o /tmp/nuvio-hold-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-hold-tests "$@"

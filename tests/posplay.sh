#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Liga o app inteiro menos o main, como tests/player.sh: posplay.c fala com
# catalogo, extras e video. Nenhuma janela e aberta — posplay_atualizar nao
# desenha, e e so ele e posplay_evento que este teste exercita.
cc "${flags[@]}" "${sources[@]}" tests/posplay.c -Isrc -o /tmp/nuvio-posplay-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-posplay-tests "$@"

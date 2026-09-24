#!/bin/bash
# A variante do tamanho do desenho: a regra (artetamanho.c) e o cache
# (texvariante.c: chave, disco e promocao a heroi). Sem rede.
#
#   bash tests/artetamanho.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/artetamanho.c tests/artetamanho.c \
  -Isrc -o /tmp/nuvio-artetamanho-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations
/tmp/nuvio-artetamanho-tests

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
# Cada tamanho do TMDB com a largura real dele (a "original" em 1600).
sips -z 439 780 tests/amostra.jpg --out /tmp/nuvio-texvariante-780.jpg >/dev/null
sips -z 720 1280 tests/amostra.jpg --out /tmp/nuvio-texvariante-1280.jpg >/dev/null
sips -z 900 1600 tests/amostra.jpg --out /tmp/nuvio-texvariante-1600.jpg >/dev/null
cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/texvariante.c -Isrc -o /tmp/nuvio-texvariante \
  -O1 -g -Wall -Wextra -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-texvariante /tmp/nuvio-texvariante-780.jpg \
  /tmp/nuvio-texvariante-1280.jpg /tmp/nuvio-texvariante-1600.jpg

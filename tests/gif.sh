#!/bin/bash
# O detector de GIF animado, offline (#29).
#
#   bash tests/gif.sh
#   SANITIZE=1 bash tests/gif.sh
#
# So src/gif.c: gif_animado nao depende de SDL, de GL nem de rede — le um
# arquivo e caminha pelos blocos. Fora do Emscripten gif_textura e um talao que
# devolve 0, entao nao ha simbolo de GL para linkar.
#
# SANITIZE=1 e o que da valor ao caso do arquivo truncado: sem ASan uma leitura
# fora do buffer passaria despercebida, que e exatamente o defeito que aquele
# caso procura.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/gif.c tests/gif.c \
  -Isrc -o /tmp/nuvio-gif-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-gif-tests

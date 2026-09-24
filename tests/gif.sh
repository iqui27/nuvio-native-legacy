#!/bin/bash
# O detector de GIF animado (#29) e o decodificador nativo com o fio de
# decode (#84), offline.
#
#   bash tests/gif.sh
#   SANITIZE=1 bash tests/gif.sh
#
# So src/gif.c: detector, decodificador e fio nao dependem de SDL, de GL nem de
# rede. Fora do Emscripten gif_textura e um talao que devolve 0, entao nao ha
# simbolo de GL para linkar.
#
# SANITIZE=1 e o que da valor aos casos de arquivo truncado e LZW com lixo: sem
# ASan uma leitura fora do buffer passaria despercebida, que e exatamente o
# defeito que aqueles casos procuram. O fio tambem passa limpo no TSan
# (-fsanitize=thread), conferido a mao.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/gif.c tests/gif.c \
  -Isrc -o /tmp/nuvio-gif-tests -O1 -g -pthread \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-gif-tests

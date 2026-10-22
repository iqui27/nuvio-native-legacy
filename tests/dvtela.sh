#!/bin/bash
# A maquina da tela do Dolby Vision em MKV (dvtela.h). Sem GL nem rede.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/dvtela.c tests/dvtela.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o /tmp/nuvio-dvtela-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lm
/tmp/nuvio-dvtela-tests

#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -Isrc \
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# tests/proximo.c inclui src/proximo.c direto: a regra nao depende de nada do
# app (nem SDL, nem catalogo.c), entao nao ha o que linkar alem dela.
cc ${flags[@]+"${flags[@]}"} tests/proximo.c -o /tmp/nuvio-proximo-tests
/tmp/nuvio-proximo-tests

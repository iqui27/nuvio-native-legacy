#!/bin/bash
# Vinculo do Simkl por perfil (simkl-p<N>.txt). Sem SDL e sem rede: disco e
# rede sao dubles em memoria; js/jsw sao os de verdade.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/simklauth.c src/js.c src/jsw.c \
  tests/simklauth_perfil.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-simklauth-tests -O1 -g -lpthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-simklauth-tests

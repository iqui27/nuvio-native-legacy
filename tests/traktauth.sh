#!/bin/bash
# Renovacao proativa do token Trakt. Sem SDL e sem rede: o teste substitui
# rede_postar_st, dados_* e trakt_* por dubles; js/jsw sao os de verdade.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# ${arr[@]+...}: o bash 3.2 do macOS chama "${flags[@]}" de unbound quando o
# vetor esta vazio e set -u esta ligado.
cc ${flags[@]+"${flags[@]}"} src/traktauth.c src/js.c src/jsw.c \
  tests/traktauth_renov.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-traktauth-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-traktauth-tests

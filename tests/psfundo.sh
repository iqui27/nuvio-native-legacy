#!/bin/bash
# Lista de arte de fundo da tela de escolha de perfil (issue #90). NAO precisa
# de rede, de SDL nem de disco: o catalogo entra em memoria por cat_definir_tudo
# e os dubles de identidade vivem em tests/psfundo.c (mesma razao de
# tests/catcache.sh).
#
#   bash tests/psfundo.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/psfundo.c src/catalogo.c src/artehero.c \
  tests/psfundo.c \
  -Isrc -o /tmp/nuvio-psfundo-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-psfundo-tests

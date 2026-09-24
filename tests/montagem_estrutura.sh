#!/bin/bash
# Estrutura (colecoes/ordem/limite) mudando no meio da montagem nao descarta o
# que chegou; identidade (dono/perfil/addons) continua descartando.
#
#   bash tests/montagem_estrutura.sh
#
# montar() de verdade (descoberta.c por #include) com homeestado.c, catalogo.c,
# colecoes.c e catordem.c de verdade. Ver o cabecalho de tests/montagem_estrutura.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} tests/montagem_estrutura.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/js.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-montagem-estrutura-tests -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
/tmp/nuvio-montagem-estrutura-tests

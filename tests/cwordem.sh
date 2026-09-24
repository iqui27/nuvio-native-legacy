#!/bin/bash
# A Ordenacao de "Continuar assistindo" faz efeito (issue #127).
#
#   bash tests/cwordem.sh
#
# Tres binarios, pelo mesmo motivo de tests/cwremover.sh (descoberta.c e home.c
# nao cabem no mesmo #include):
#   tests/cwordem.c       — a regra pura (src/cwordem.c): ordem por modo,
#                           estreias, conjunto de futuros;
#   tests/cwordem_desc.c  — montarContinuar com um "Trakt" falso: a ordem que
#                           sai em cada modo e o showUnairedNextUp;
#   tests/cwordem_home.c  — a HOME parte a retomada e poe "Proximos episodios"
#                           logo abaixo dela so em "Separar futuros".
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=/tmp
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/cwordem.c -Isrc \
  -o "$out/nuvio-cwordem-tests" -O1 -g -Wall -Wextra
"$out/nuvio-cwordem-tests"
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/cwordem.c tests/cwordem_desc.c \
  src/cotacat.c src/js.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$out/nuvio-cwordem-desc-tests" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$out/nuvio-cwordem-desc-tests"
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/colecoes.c \
  src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c tests/cwordem_home.c \
  -Isrc -o "$out/nuvio-cwordem-home-tests" -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
"$out/nuvio-cwordem-home-tests"
echo "cwordem: tudo ok"

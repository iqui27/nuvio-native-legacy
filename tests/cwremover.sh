#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# "Tirar de Continuar assistindo" sai NA HORA e nao volta com o remoto velho.
#
#   bash tests/cwremover.sh
#
# Dois binarios, porque descoberta.c e home.c nao cabem no mesmo #include:
#   tests/cwremover.c      — fileira publicada, refacao com o "Trakt" falso
#                            (paused_at mais velho fica fora, mais novo volta),
#                            publicacao em voo, remontagem sem rede;
#   tests/cwremover_home.c tests/amigosfil_stub.c tests/home_boundary_stub.c — a HOME remonta no mesmo quadro (guarda curto da
#                            1.4 em sincronizarFileiras).
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/discovery_boundary_stub.c ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/cwordem.c src/cwretido.c tests/cwremover.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o ${TMPDIR:-/tmp}/nuvio-cwremover-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
${TMPDIR:-/tmp}/nuvio-cwremover-tests
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/metaprov.c src/catordem.c src/fileiras.c src/homefil.c src/artehero.c src/cwordem.c src/cwretido.c src/fonteregra.c src/regexjs.c src/ajlog.c src/curadoria.c tests/cwremover_home.c tests/amigosfil_stub.c tests/home_boundary_stub.c \
  -Isrc -DNV_SHOT_HOOKS -DCURADORIA_TESTE -o ${TMPDIR:-/tmp}/nuvio-cwremover-home-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-cwremover-home-tests

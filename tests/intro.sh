#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Marcadores do TheIntroDB: leitura da resposta.
#
#   bash tests/intro.sh
#
# Leitores de intro e capitulos — sem rede (as cargas do teste
# sao respostas reais capturadas com curl) nem de SDL.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/intro.c src/js.c src/credfonte.c src/capmkv.c src/mkv.c tests/intro.c \
  -DNV_SHOT_HOOKS -Isrc -o ${TMPDIR:-/tmp}/nuvio-intro-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
${TMPDIR:-/tmp}/nuvio-intro-tests

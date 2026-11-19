#!/bin/bash
# Corrida entre quem escreve e quem le o mapa de episodios vistos. Ver
# tests/vistoep_corrida.c.
#   bash tests/vistoep_corrida.sh            (sem sanitizador: so os asserts)
#   SANITIZE=1 bash tests/vistoep_corrida.sh (ASAN+UBSAN)
#   SANITIZE=thread bash tests/vistoep_corrida.sh (TSAN)
set -eu
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-vistoep-corrida"
cc ${flags[@]+"${flags[@]}"} src/vistoep.c src/js.c tests/vistoep_corrida.c \
  -Isrc -o "$bin" -O1 -g -Wall -lpthread
"$bin"

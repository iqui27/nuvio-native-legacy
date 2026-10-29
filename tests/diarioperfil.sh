#!/bin/bash
# Perfil e Stats sem Trakt (diarioperfil.c) sobre o Diario de verdade.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out="${TMPDIR:-/tmp}/nuvio-diarioperfil-tests"
cc ${flags[@]+"${flags[@]}"} src/diarioperfil.c src/diario.c src/diarionomes.c tests/diarioperfil.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o "$out" -O1 -g -Wall -Wextra \
  -Wno-unused-parameter -Wno-deprecated-declarations
TZ=America/Sao_Paulo "$out"

#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Escolha do item "continuar" por perfil (perfilcont.c + prog_continuar_de_perfil).
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Wall -Wextra -Isrc -Wno-macro-redefined -Wno-deprecated-declarations -Wno-unused-parameter -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/progresso.c src/perfilcont.c tests/perfilcont.c -o ${TMPDIR:-/tmp}/nuvio-perfilcont-tests
${TMPDIR:-/tmp}/nuvio-perfilcont-tests

#!/bin/bash
# Piso do Continuar assistindo (#326): prog_continuar_de_perfil_min.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cwminimo.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -Isrc -Wno-macro-redefined -Wno-deprecated-declarations -Wno-unused-parameter -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/progresso.c tests/cwminimo.c -o "$dir/teste"
"$dir/teste"

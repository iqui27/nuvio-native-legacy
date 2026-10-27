#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-fontescanais-folha.XXXXXX")
trap 'rm -f "$out"' EXIT
read -r -a sdlflags <<< "$(pkg-config --cflags sdl2)"
cc "${flags[@]}" -DNV_TOUCH_UI -std=gnu11 -Isrc "${sdlflags[@]}" -O1 -g -Wall -Wextra -Wno-unused-function \
  tests/fontescanais_folha.c src/fontescanais.c src/streamfit.c src/vazao.c src/badges.c src/fonteauto.c src/fontenota.c src/vaitocar.c \
  src/fontefalha.c src/fontefiltro.c src/linguas.c src/limpa.c src/dobra.c -lm -o "$out"
"$out"

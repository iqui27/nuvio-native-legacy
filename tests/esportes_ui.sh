#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-esportes-ui.XXXXXX")
trap 'rm -f "$out"' EXIT
read -r -a sdlflags <<< "$(pkg-config --cflags sdl2)"
cc "${flags[@]}" -DNV_TOUCH_UI -std=gnu11 -Isrc "${sdlflags[@]}" -O1 -g -Wall -Wextra -Wno-unused-function \
  tests/esportes_ui.c src/esportes_parse.c src/fontescanais.c src/ctxlista.c src/js.c src/buscanorm.c -lm -o "$out"
"$out"

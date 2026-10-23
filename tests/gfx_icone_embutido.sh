#!/bin/bash
# Icone essencial no nucleo TPK atualizado, sem janela GL nem TV.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-icone-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-deprecated-declarations \
  -include tests/gfx_icone_embutido_gl.h tests/gfx_icone_embutido.c src/gfx.c \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -framework OpenGL -o "$tmp/teste"
"$tmp/teste"

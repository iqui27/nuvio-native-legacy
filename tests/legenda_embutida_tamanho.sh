#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
if [ "$(uname)" = Darwin ]; then gc=-Wl,-dead_strip; else gc=-Wl,--gc-sections; fi
cc tests/legenda_embutida_tamanho.c src/faixasmkv.c -Isrc "${sdl_flags[@]}" \
  -std=gnu11 -O1 -g -ffunction-sections -fdata-sections "$gc" \
  -Wall -Wextra -Wno-unused-function -o "$work/test"
"$work/test"

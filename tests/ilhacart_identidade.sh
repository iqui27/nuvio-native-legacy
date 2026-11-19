#!/bin/bash
# Fixture puro de identidade: nao usa renderer, TV, disco pessoal ou rede.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ilhacart-identidade.XXXXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined -ffunction-sections)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections -lm); fi
cc "${flags[@]}" tests/ilhacart_identidade.c src/ilhacart.c -o "$dir/test"
"$dir/test"

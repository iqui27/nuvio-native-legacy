#!/bin/bash
# Medidor de desempenho fora da tela enquanto o trailer toca (#338). Ver o .c.
# Fixture puro: sem renderer, TV, disco pessoal ou rede.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ilha-medidor-trailer.XXXXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function)
if [ "$(uname -s)" = Darwin ]; then flags+=(-L/opt/homebrew/lib -lSDL2 -Wl,-undefined,dynamic_lookup -lm); else flags+=(-lm); fi
cc "${flags[@]}" tests/ilha_medidor_trailer.c -o "$dir/test"
"$dir/test"
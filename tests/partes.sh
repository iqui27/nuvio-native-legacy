#!/bin/bash
# "Arquivos desta fonte" e o filme em partes (ver tests/partes.c).
set -eu
cd "$(dirname "$0")/.."
inc=(-Isrc)
if [ "$(uname)" = Darwin ]; then inc+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
else read -r -a f <<< "$(pkg-config --cflags sdl2)"; inc+=("${f[@]}"); fi
cc -O1 -g -Wall "${inc[@]}" tests/partes.c src/partes.c src/js.c -lpthread -lm -o "${TMPDIR:-/tmp}/nuvio-partes"
"${TMPDIR:-/tmp}/nuvio-partes"
cc -O1 -g -Wall "${inc[@]}" tests/partes-account.c src/js.c -lpthread -lm -o "${TMPDIR:-/tmp}/nuvio-partes-account"
"${TMPDIR:-/tmp}/nuvio-partes-account"

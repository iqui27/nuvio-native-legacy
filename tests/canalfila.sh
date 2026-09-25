#!/bin/bash
# Fila de fontes de canal ao vivo contra servidor local. Ver tests/canalfila.c.
set -eu
cd "$(dirname "$0")/.."
PORTA=18771
python3 tests/canalfila.py $PORTA & SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT
sources=()
for source in src/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/canalfila.c -Isrc -o /tmp/nuvio-canalfila \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
sleep 0.5
/tmp/nuvio-canalfila $PORTA

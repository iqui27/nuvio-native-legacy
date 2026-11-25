#!/bin/bash
# Formatter icons skip the network queues of the shared texture cache.
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-texicones.XXXXXX")
trap 'rm -rf "$D"' EXIT
sources=()
for s in src/*.c src/dts/*.c; do [[ "$s" != src/main.c ]] && sources+=("$s"); done
if [[ $(uname -s) == Linux ]]; then
  read -ra CF <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2)"
  read -ra LB <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2)"
  cc "${sources[@]}" tests/tex_icones.c -Isrc -o "$D/t" -O1 -g -DNV_LINUX_DESKTOP "${CF[@]}" "${LB[@]}" -lz -lm -ldl -lpthread
else
  cc "${sources[@]}" tests/tex_icones.c -Isrc -o "$D/t" -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
fi
NUVIO_DADOS="$D" "$D/t"

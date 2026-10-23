#!/bin/bash
# Ajuste Teclado na LG (-DNV_WEBOS). Ver o .c.
#
#   bash tests/teclado_webos.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc -DNV_TESTE -DNV_WEBOS "${sources[@]}" tests/teclado_webos.c -Isrc -o /tmp/nuvio-teclado-webos \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-teclado-webos

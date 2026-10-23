#!/bin/bash
# The same footer/touch test without NV_TOUCH_UI: the TV/desktop build has no
# touch UI and the pointer is the mouse (Magic Remote). One serial build, like
# every other test script.
#   bash tests/ajustes_ux_toque_modo_desktop.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
NUVIO_DADOS="$(mktemp -d /tmp/nuvio-ajustes-ux-toque-modo-desktop.XXXXXX)"
export NUVIO_DADOS
binary="$NUVIO_DADOS/teste"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
cc "${sources[@]}" tests/ajustes_ux_toque_modo.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"

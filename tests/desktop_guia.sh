#!/bin/bash
# Desktop TV Guide clears the native sidebar; TV geometry unchanged. See the .c.
#   bash tests/desktop_guia.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-desktop-guia.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
for flags in "-DNV_TOUCH_UI -DNV_DESKTOP_UI -DNV_DESKTOP_RELEASE" ""; do
  cc "${sources[@]}" tests/desktop_guia.c -Isrc -o "$work/test" $flags \
    -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined -Wl,-undefined,dynamic_lookup
  "$work/test"
done

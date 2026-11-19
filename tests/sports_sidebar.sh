#!/bin/bash
# Offline native state proof and 1920x1080 captures, without input events.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sports-sidebar.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
out=/Volumes/ExternalSSD/tmp/rev/sports-sidebar
mkdir -p "$NUVIO_DADOS" "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/menu.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
sources+=(tests/sports_sidebar.c tests/sports_sidebar_menu.c)
make -s -j4 -f tools/native-objects.mk SOURCES="${sources[*]}" OBJDIR="$work/objects" CC=cc \
  NATIVE_FLAGS='-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations'
objects=()
for source in "${sources[@]}"; do objects+=("$work/objects/${source%.c}.o"); done
cc "${objects[@]}" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/check"
"$work/check" "$out"

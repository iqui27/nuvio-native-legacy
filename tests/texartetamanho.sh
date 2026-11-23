#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-texartetamanho.XXXXXX")
trap 'rm -f "$out"' EXIT
cc "${sources[@]}" tests/texartetamanho.c -Isrc -o "$out" -O1 -g \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib) \
  -ldl -pthread -lm
"$out"

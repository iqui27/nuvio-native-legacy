#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
pkg-config --exists libass sdl2 fribidi
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then
  flags=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
cc -DNV_ASS_LIBASS -include tests/ass_pisca_gl.h -Isrc \
  "${flags[@]}" tests/plainass.c src/plainass.c src/legenda.c src/assrender.c \
  -o "$dir/test" $(pkg-config --cflags --libs libass sdl2 fribidi) -lm -pthread
"$dir/test"

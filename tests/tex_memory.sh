#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 tests/tex_memory.py
out=$(mktemp "${TMPDIR:-/tmp}/nv-tex-memory.XXXXXX")
trap 'rm -f "$out"' EXIT
cc tests/tex_memory.c src/perfiltv.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -ljpeg -lpthread -o "$out"
"$out"

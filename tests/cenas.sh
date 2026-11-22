#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall ${SANITIZE:+-fsanitize=address,undefined} -Isrc $(pkg-config --cflags sdl2) \
  src/cenas.c src/marcacena.c tests/cenas.c -o "${TMPDIR:-/tmp}/nv-cenas"
"${TMPDIR:-/tmp}/nv-cenas"

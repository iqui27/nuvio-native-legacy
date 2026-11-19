#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# The remembered source keeps the release name (the Scandal bug): two works
# with the same provider, bingeGroup, label and height must not share one
# memory row, and a re-pick differing only in the file name must be written.
#
#   bash tests/fontepref_file.sh
#
# Compiles everything EXCEPT src/fontepref.c, which the test includes (same
# recipe as tests/fontepref.sh). NUVIO_DADOS points at a temporary folder and
# the test refuses to run without it.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fontepref-file.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/fontepref.c) continue;; esac
  sources+=("$source")
done

cflags=()
libs=()
if [ "$(uname -s)" = Darwin ]; then
  cflags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -framework OpenGL)
  libs=(-lSDL2 -lSDL2_image -lSDL2_ttf -lz)
else
  cflags+=($(pkg-config --cflags sdl2 SDL2_image SDL2_ttf))
  libs=(-lSDL2 -lSDL2_image -lSDL2_ttf -lz -lGL -lm -ldl -lpthread)
fi
if [ "${SANITIZE:-0}" = 1 ]; then cflags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi

# Libs AFTER the objects: the linker is order-sensitive on Linux.
cc ${cflags[@]+"${cflags[@]}"} "${sources[@]}" tests/fontepref_file.c -Isrc \
  -o ${TMPDIR:-/tmp}/nuvio-fontepref-file -O1 -g \
  ${libs[@]+"${libs[@]}"} \
  -Wall -Wextra -Wno-deprecated-declarations
${TMPDIR:-/tmp}/nuvio-fontepref-file "$@"

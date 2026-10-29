#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "$TMPDIR/nuvio-detail-cast-ok.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
export NUVIO_DADOS="$test_dir/data"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/detail.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/detail_cast_ok.c -Isrc -o "$test_dir/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$test_dir/test"

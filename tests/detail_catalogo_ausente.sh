#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
run=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detail-ausente.XXXXXX")
trap 'rm -rf "$run"' EXIT
export NUVIO_DADOS="$run/dados"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/detail.c ]; then sources+=("$source"); fi
done
flags=()
case "${SANITIZE:-}" in
  address) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac
cc "${sources[@]}" tests/detail_catalogo_ausente.c -Isrc -o "$run/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined "${flags[@]}"
"$run/test"

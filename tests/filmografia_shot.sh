#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
NUVIO_DADOS=$(mktemp -d "$TMPDIR/filmo-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
bin="$TMPDIR/filmo-shot"
cc -DAJUSTES_TESTE "${sources[@]}" tests/filmografia_shot.c -Isrc -o "$bin" \
 -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
 -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"$bin" /Volumes/ExternalSSD/tmp/rev/filmo-impl

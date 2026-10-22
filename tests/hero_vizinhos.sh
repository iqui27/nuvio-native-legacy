#!/bin/bash
# Arte dos vizinhos do hero da home (2.0.3). Ver tests/hero_vizinhos.c.
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-hero-vizinhos.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/hero_vizinhos.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-hero-vizinhos-tests" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-hero-vizinhos-tests"
echo "PASS: vizinhos do hero aquecidos; a virada manual acha a arte pronta"

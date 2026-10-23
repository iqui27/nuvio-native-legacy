#!/bin/bash
# Arte dos vizinhos do carrossel (2.0.3). Ver tests/carrossel_heroi.c.
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-carrossel-heroi.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/carrossel_heroi.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-carrossel-heroi-tests" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-carrossel-heroi-tests"
echo "PASS: vizinho aquecido sem furar a fila; o titulo que chega passa na frente"

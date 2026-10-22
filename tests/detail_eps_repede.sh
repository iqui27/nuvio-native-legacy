#!/bin/bash
# Pagina de serie pede/repede a lista de episodios (2.0.3). Ver tests/detail_eps_repede.c.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-detail-eps-repede.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/detail_eps_repede.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-detail-eps-repede-tests" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-detail-eps-repede-tests"
echo "PASS: pagina de serie pede a lista na abertura e depois da volta do vetor"

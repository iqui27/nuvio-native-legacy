#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Captura da folha de fontes com emoji/bandeira/versalete de addon (issue #144).
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/fontes_glifos_shot.sh /tmp/nuvio-fontes-glifos
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fontes-glifos-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/fontes_glifos_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-fontes-glifos-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-fontes-glifos-shot "$@"

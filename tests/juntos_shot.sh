#!/bin/bash
# Capturas do Watch Together (juntosui). Nao entra na suite.
#   bash tests/juntos_shot.sh [prefixo]
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/juntos.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/juntos_shot.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-juntos-shot" \
  -O1 -g -DNV_LEVE -DNV_SHOT_HOOKS -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -w
"${TMPDIR:-/tmp}/nuvio-juntos-shot" "${1:-${TMPDIR:-/tmp}/nuvio-juntos}"

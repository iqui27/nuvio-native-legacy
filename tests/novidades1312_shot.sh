#!/bin/bash
# Capturas visuais do cartao 1.3.12 em portugues, ingles e animacoes reduzidas.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-n1312-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades1312_shot.c -Isrc \
  -o /tmp/nuvio-n1312-shot -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-n1312-shot "${1:-/tmp/nuvio-novidades1312}"

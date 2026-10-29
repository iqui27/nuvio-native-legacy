#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Ranking da colecao e topo da grade focada, em PNG (job 205-colrank). Nao
# entra na suite (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL.
#
#   bash tests/vertudo_colrank_shot.sh /tmp/nuvio-colrank
set -eu
cd "$(dirname "$0")/.."

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/vertudo_colrank_shot.c -Isrc -Itests -o "${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-colrank-shot}" \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-colrank-shot}" "$@"

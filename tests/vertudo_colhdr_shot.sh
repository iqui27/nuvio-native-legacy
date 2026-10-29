#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Cabecalho da colecao, direcao A (job 205-colhdrlinha): PNG + geometria. Nao
# entra na suite (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL.
#
#   bash tests/vertudo_colhdr_shot.sh /Volumes/ExternalSSD/tmp/rev/colhdrA/depois
set -eu
cd "$(dirname "$0")/.."

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/vertudo_colhdr_shot.c -Isrc -Itests -o "${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-colhdr-shot}" \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
saida="${1:-$TMPDIR/nuvio-colhdr}"
mkdir -p "$saida"
"${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-colhdr-shot}" "$saida"

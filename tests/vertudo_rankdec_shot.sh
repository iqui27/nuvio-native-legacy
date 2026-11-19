#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Ranking e Decadas da pagina de streaming, em PNG (job 205-rankdecadas). Nao
# entra na suite (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL.
#
#   bash tests/vertudo_rankdec_shot.sh /tmp/nuvio-rankdec
set -eu
cd "$(dirname "$0")/.."

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/vertudo_rankdec_shot.c -Isrc -Itests -o "${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-rankdec-shot}" \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-rankdec-shot}" "$@"

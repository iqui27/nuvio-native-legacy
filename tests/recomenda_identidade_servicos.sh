#!/bin/bash
# Simkl e Letterboxd ligados a pessoa (F08).
#   bash tests/recomenda_identidade_servicos.sh
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-recident-dados.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/recomenda_identidade_servicos.c -Isrc -DNV_REC_URL='"http://127.0.0.1:8799"' -o "$NUVIO_DADOS/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1 || { cat "$NUVIO_DADOS/build.log" >&2; exit 1; }
grep -E "recomenda(_social)?\.c.*(error|warning)" "$NUVIO_DADOS/build.log" || true
"$NUVIO_DADOS/teste"

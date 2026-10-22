#!/bin/bash
# Pedidos de amizade lidos a cada ciclo do fio social. Ver o .c.
#   bash tests/recomenda_pedidos_ciclo.sh            (o recomenda.c da arvore)
#   REC_C=/caminho/recomenda.c bash tests/...       (outro, ex.: o do master)
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pedciclo-dados.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
extra=()
if [ -n "${REC_C:-}" ]; then extra+=("-DREC_C=\"$REC_C\""); fi
cc "${sources[@]}" tests/recomenda_pedidos_ciclo.c -Isrc "${extra[@]+"${extra[@]}"}" -o "$NUVIO_DADOS/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1 || { cat "$NUVIO_DADOS/build.log" >&2; exit 1; }
"$NUVIO_DADOS/teste"

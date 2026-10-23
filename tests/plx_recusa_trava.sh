#!/bin/bash
# Recusa no pedido (plx-recusa) nao pode deixar o mutex do cache preso: ver
# tests/plx_recusa_trava.c. Roda o quadro de Home com e sem a recusa e falha
# se cartaz ou icone nao chegam.
#   bash tests/plx_recusa_trava.sh [/caminho/captura.png]
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
mkdir -p "$TMPDIR"
NUVIO_DADOS=$(mktemp -d "$TMPDIR/nuvio-plx-trava.XXXXXX")
export NUVIO_DADOS
out="$NUVIO_DADOS/bin"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/plx_recusa_trava.c -Isrc -o "$out" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NV_SEM_RECUSA=1 "$out"
"$out" "${1:-}"

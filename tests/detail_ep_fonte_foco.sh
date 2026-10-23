#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Issue #513 (P1): o pedido de folha pelo card de episodio trava T/E do card —
# a folha rouba o foco e a escolha tocava o episodio em andamento.
# Ver tests/detail_ep_fonte_foco.c.
#
#   bash tests/detail_ep_fonte_foco.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/detail_ep_fonte_foco.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-detail-epfoco-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-detail-epfoco-tests

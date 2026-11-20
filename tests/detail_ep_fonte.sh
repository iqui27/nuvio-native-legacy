#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Issue #513: o toque no card de episodio obedece "Escolher a fonte ao
# reproduzir" — com o auto-play desligado a folha de fontes abre mesmo para
# serie em "Continuar assistindo". Ver tests/detail_ep_fonte.c.
#
#   bash tests/detail_ep_fonte.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/detail_ep_fonte.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-detail-epfonte-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-detail-epfonte-tests

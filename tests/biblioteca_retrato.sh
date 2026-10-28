#!/bin/bash
# Grade da Biblioteca iOS (tests/biblioteca_retrato.c): fechado 4x78 pt e aberto
# 8x92 pt (forcado por -DNV_BIB_AMPLO). Janela GL escondida; sem rede.
#
#   bash tests/biblioteca_retrato.sh [pasta-de-capturas]
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-biblioteca-retrato.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/biblioteca_retrato.c -Isrc -o "$NUVIO_DADOS/teste" \
  -O1 -g -DNV_SHOT_HOOKS -DNV_TOUCH_UI -DNV_BIB_AMPLO -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  tail -40 "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
"$NUVIO_DADOS/teste" fechado "$@"
"$NUVIO_DADOS/teste" aberto "$@"

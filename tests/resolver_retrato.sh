#!/bin/bash
# Resolver em retrato touch: escala cabe no canvas. Sem janela.
#   bash tests/resolver_retrato.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
NUVIO_DADOS="$(mktemp -d /tmp/nuvio-resolver-retrato.XXXXXX)"
export NUVIO_DADOS
binary="$NUVIO_DADOS/teste"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
cc "${sources[@]}" tests/resolver_retrato.c -DNV_TOUCH_UI -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"

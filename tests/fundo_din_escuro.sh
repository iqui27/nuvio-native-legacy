#!/bin/bash
# Host acceptance and 1080p captures; no network or device needed.
set -eu
cd "$(dirname "$0")/.."
ulimit -c 0
export NUVIO_DADOS
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homelayouts-shot.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS"' EXIT
export NV_FUNDO_ESCURO_OUT="${NV_FUNDO_ESCURO_OUT:-/Volumes/ExternalSSD/tmp/rev/fundoescuro}"
mkdir -p "$NV_FUNDO_ESCURO_OUT"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/homelayouts_shot.c tests/fundo_din_escuro.c \
  -DNV_FUNDO_ESCURO_TEST -Isrc -o "$NUVIO_DADOS/test" -O1 \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$NUVIO_DADOS/test"
for name in cinza cinza-escuro; do
  sips -s format png "$NV_FUNDO_ESCURO_OUT/$name.bmp" --out "$NV_FUNDO_ESCURO_OUT/$name.png" >/dev/null
  rm "$NV_FUNDO_ESCURO_OUT/$name.bmp"
done

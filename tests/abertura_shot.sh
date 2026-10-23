#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Abertura do app (2.0 N1): tempos dos tres estilos + capturas dos dois logos.
# Precisa de janela GL. Saida: prefixo em $1 (padrao /tmp/nuvio-abertura).
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
BIN="${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-abertura-shot}"
cc -DAJUSTES_TESTE "${sources[@]}" tests/abertura_shot.c -Isrc -o "$BIN" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$BIN" "$@"

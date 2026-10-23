#!/bin/bash
# A conta do grafico de temporadas (src/temporadas_grafico.c), sem GL. Escreve so em NUVIO_DADOS.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-temporadas_grafico-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/temporadas_grafico.c ]; then sources+=("$source"); fi
done
# Conta entradas em tgraf_montar apenas neste objeto, sem alterar a producao.
cc -c src/temporadas_grafico.c -Isrc -I/opt/homebrew/include -O1 -g \
  -finstrument-functions -o "$tmp/grafico.o"
cc "${sources[@]}" "$tmp/grafico.o" tests/temporadas_grafico.c -Isrc -o "$tmp/teste" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -w
mkdir -p "$tmp/dados"
NUVIO_DADOS="$tmp/dados" "$tmp/teste" 2>&1 | grep -E 'temporadas_grafico|Assert|assert'

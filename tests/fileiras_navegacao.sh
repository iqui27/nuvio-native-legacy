#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Navegacao por controle na folha "Fileiras da Home", sem janela nem rede:
# os chips da barra ficam no topo, como as abas (barraPos/ajFilBarra), e nao
# no fim da lista. Leve: so estado + eventos, sem desenho.
#   bash tests/fileiras_navegacao.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
NUVIO_DADOS="$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fileiras-navegacao.XXXXXX)"
export NUVIO_DADOS
binary="$NUVIO_DADOS/teste"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
cc "${sources[@]}" tests/fileiras_navegacao.c -Isrc -o "$binary" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"

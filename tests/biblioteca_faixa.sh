#!/bin/bash
# Faixa de filtros da Biblioteca (tests/biblioteca_faixa.c): em 1920x1080, topo
# a 120%, Inter e Origem "Conta Nuvio", abas e chips cabem na area util. Janela
# GL escondida do Mac; sem rede.
#
#   bash tests/biblioteca_faixa.sh
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-biblioteca-faixa.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/biblioteca_faixa.c -Isrc -o "$NUVIO_DADOS/teste" \
  -O1 -g -DNV_SHOT_HOOKS -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
"$NUVIO_DADOS/teste" "$@"

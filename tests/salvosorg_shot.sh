#!/bin/bash
# Organizar os Salvos: categorias, ordenar, agrupar, estilo e Social por
# pessoa, com capturas PNG para OLHAR. Precisa de GL (janela escondida);
# escreve so em NUVIO_DADOS, uma pasta temporaria. Ver tests/salvosorg_shot.c.
#
#   bash tests/salvosorg_shot.sh [pasta-das-capturas]
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-salvosorg-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
saida="${1:-$tmp/capturas}"
mkdir -p "$saida" "$tmp/dados"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/salvosorg_shot.c -Isrc -o "$tmp/teste" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" "$tmp/teste" "$saida" | grep -E "^ |^$|^[a-z].*:$|PASSOU|FALHOU"

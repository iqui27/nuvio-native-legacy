#!/bin/bash
# GUIA TV v5 — captura da tela principal do guia em PNG pt-BR + comparacoes
# lado a lado com o mock aprovado. Nao entra na suite automatica (GL + janela).
#   bash tests/guiav5_principal_shot.sh [saida]
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/guiav5-png/principal}
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-g5-principal.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/guiav5_principal_shot.c -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/shot" "$out"
python3 tests/guiav5_compare.py "$out"

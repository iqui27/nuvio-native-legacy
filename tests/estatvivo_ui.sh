#!/bin/bash
# Estatisticas ao vivo (#338): o OK que abriu o cartao nao o fixa no canto e os
# tres tamanhos (Completo, Compacto, Minimo) com OK/setas, lembrar o ultimo e o
# tamanho dos Ajustes. Sem GL. Ver tests/estatvivo_ui.c.
#   bash tests/estatvivo_ui.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-estatvivo-ui.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
flags=()
if [ "${1:-}" = --before ]; then
  git show 3fc35494:src/estatvivoui.c > "$work/estatvivoui.c"
  for i in "${!sources[@]}"; do
    [ "${sources[$i]}" != src/estatvivoui.c ] || sources[$i]="$work/estatvivoui.c"
  done
  flags+=(-DANTES)
fi
cc "${flags[@]}" "${sources[@]}" tests/estatvivo_ui.c -Isrc -o "$work/test" -DESTATVIVO_TESTE -DCENTRAL_TESTE \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DADOS="$work/dados" "$work/test"
